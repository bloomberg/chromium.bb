// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MACHINE_INTELLIGENCE_GENERIC_LOGISTIC_REGRESSION_INFERENCE_H_
#define COMPONENTS_MACHINE_INTELLIGENCE_GENERIC_LOGISTIC_REGRESSION_INFERENCE_H_

#include "components/machine_intelligence/proto/generic_logistic_regression_model.pb.h"
#include "components/machine_intelligence/proto/ranker_example.pb.h"

namespace machine_intelligence {

float Sigmoid(float x);

// TODO(hamelphi): Implement an interface base class
// BinaryClassificationInferenceModule.
//
// Implements inference for a GenericLogisticRegressionModel.
class GenericLogisticRegressionInference {
 public:
  explicit GenericLogisticRegressionInference(
      GenericLogisticRegressionModel model_proto);
  // Returns a boolean decision given a RankerExample. Uses the same logic as
  // PredictScore, and then applies the model decision threshold.
  bool Predict(const RankerExample& example);
  // Returns a score between 0 and 1 give a RankerExample.
  float PredictScore(const RankerExample& example);

 private:
  // Returns the decision threshold. If no threshold is specified in the proto,
  // use 0.5.
  float GetThreshold();
  const GenericLogisticRegressionModel proto_;
};

}  // namespace machine_intelligence
#endif  // COMPONENTS_MACHINE_INTELLIGENCE_GENERIC_LOGISTIC_REGRESSION_INFERENCE_H_
