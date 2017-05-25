// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2015 Qianyi Zhou <Qianyi.Zhou@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include "Feature.h"

#include <Core/Utility/Console.h>
#include <Core/Geometry/PointCloud.h>
#include <Core/Geometry/KDTreeFlann.h>

namespace three {

namespace {

Eigen::Vector4d ComputePairFeatures(const Eigen::Vector3d &p1,
		const Eigen::Vector3d &n1, const Eigen::Vector3d &p2,
		const Eigen::Vector3d &n2)
{
	Eigen::Vector4d result;
	auto dp2p1 = p2 - p1;
	result(3) = dp2p1.norm();
	if (result(3) == 0.0) {
		return Eigen::Vector4d::Zero();
	}
	auto n1_copy = n1;
	auto n2_copy = n2;
	double angle1 = n1_copy.dot(dp2p1) / result(3);
	double angle2 = n2_copy.dot(dp2p1) / result(3);
	if (acos(fabs(angle1)) > acos(fabs(angle2))) {
		n1_copy = n2;
		n2_copy = n1;
		dp2p1 *= -1.0;
		result(2) = -angle2;
	} else {
		result(2) = angle1;
	}
	auto v = dp2p1.cross3(n1_copy);
	double v_norm = v.norm();
	if (v_norm == 0.0) {
		return Eigen::Vector4d::Zero();
	}
	v /= v_norm;
	auto w = n1_copy.cross3(v);
	result(1) = v.dot(n2_copy);
	result(0) = atan2(w.dot(n2_copy), n1_copy.dot(n2_copy));
	return result;
}

std::shared_ptr<Feature> ComputeSPFHFeature(const PointCloud &input,
		const KDTreeFlann &kdtree, const KDTreeSearchParam &search_param)
{
	auto feature = std::make_shared<Feature>();
	feature->Resize(33, input.points_.size());
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
	for (int i = 0; i < (int)input.points_.size(); i++) {
		const auto &point = input.points_[i];
		const auto &normal = input.normals_[i];
		std::vector<int> indices;
		std::vector<double> distance2;
		kdtree.Search(point, search_param, indices, distance2);
		if (indices.size() > 1) {
			// only compute SPFH feature when a point has neighbors
			double hist_incr = 100.0 / (double)(indices.size() - 1);
			for (size_t k = 1; k < indices.size(); k++) {
				// skip the point itself, compute histogram
				auto pf = ComputePairFeatures(point, normal,
						input.points_[indices[k]], input.normals_[indices[k]]);
				int h_index = (int)(floor(11 * (pf(0) + M_PI) / (2.0 * M_PI)));
				if (h_index < 0) h_index = 0;
				if (h_index >= 11) h_index = 10;
				feature->data_(h_index, i) += hist_incr;
				h_index = (int)(floor(11 * (pf(1) + 1.0) * 0.5));
				if (h_index < 0) h_index = 0;
				if (h_index >= 11) h_index = 10;
				feature->data_(h_index + 11, i) += hist_incr;
				h_index = (int)(floor(11 * (pf(2) + 1.0) * 0.5));
				if (h_index < 0) h_index = 0;
				if (h_index >= 11) h_index = 10;
				feature->data_(h_index + 22, i) += hist_incr;
			}
		}
	}
	return feature;
}

}	// unnamed namespace

std::shared_ptr<Feature> ComputeFPFHFeature(const PointCloud &input,
		const KDTreeSearchParam &search_param/* = KDTreeSearchParamKNN()*/)
{
	auto feature = std::make_shared<Feature>();
	feature->Resize(33, input.points_.size());
	if (input.HasNormals() == false) {
		PrintDebug("[ComputeFPFHFeature] Failed because input point cloud has no normal.\n");
		return feature;
	}
	KDTreeFlann kdtree(input);
	auto spfh = ComputeSPFHFeature(input, kdtree, search_param);
	
	return feature;
}

}	// namespace three
