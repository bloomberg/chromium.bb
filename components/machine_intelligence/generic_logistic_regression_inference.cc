// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/machine_intelligence/generic_logistic_regression_inference.h"

#include <cmath>

#include "base/logging.h"
#include "components/machine_intelligence/ranker_example_util.h"

namespace machine_intelligence {

const float kDefaultThreshold = 0.5f;

float Sigmoid(float x) {
  return 1.0f / (1.0f + exp(-x));
}

GenericLogisticRegressionInference::GenericLogisticRegressionInference(
    GenericLogisticRegressionModel model_proto)
    : proto_(std::move(model_proto)) {}

float GenericLogisticRegressionInference::PredictScore(
    const RankerExample& example) {
  float activation = 0.0f;
  for (const auto& weight_it : proto_.weights()) {
    const std::string& feature_name = weight_it.first;
    const FeatureWeight& feature_weight = weight_it.second;
    switch (feature_weight.feature_type_case()) {
      case FeatureWeight::FEATURE_TYPE_NOT_SET: {
        DVLOG(1) << "Feature type not set for " << feature_name;
        break;
      }
      case FeatureWeight::kScalar: {
        float value;
        if (GetFeatureValueAsFloat(feature_name, example, &value)) {
          const float weight = feature_weight.scalar();
          activation += value * weight;
        }
        break;
      }
      case FeatureWeight::kOneHot: {
        std::string value;
        if (GetOneHotValue(feature_name, example, &value)) {
          const auto& category_weights = feature_weight.one_hot().weights();
          auto category_it = category_weights.find(value);
          if (category_it != category_weights.end()) {
            activation += category_it->second;
          } else {
            // If the category is not found, use the default weight.
            activation += feature_weight.one_hot().default_weight();
          }
        } else {
          // If the feature is missing, use the default weight.
          activation += feature_weight.one_hot().default_weight();
        }
        break;
      }
      case FeatureWeight::kSparse: {
        DVLOG(1) << "Sparse features not implemented yet.";
        break;
      }
      case FeatureWeight::kBucketized: {
        DVLOG(1) << "Bucketized features not implemented yet.";
        break;
      }
    }
  }
  return Sigmoid(proto_.bias() + activation);
}

bool GenericLogisticRegressionInference::Predict(const RankerExample& example) {
  return PredictScore(example) >= GetThreshold();
}

float GenericLogisticRegressionInference::GetThreshold() {
  return (proto_.has_threshold() && proto_.threshold() > 0) ? proto_.threshold()
                                                            : kDefaultThreshold;
}

}  // namespace machine_intelligence
