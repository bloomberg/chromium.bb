// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MACHINE_INTELLIGENCE_BINARY_CLASSIFIER_PREDICTOR_H_
#define COMPONENTS_MACHINE_INTELLIGENCE_BINARY_CLASSIFIER_PREDICTOR_H_

#include "base/compiler_specific.h"
#include "components/machine_intelligence/base_predictor.h"
#include "components/machine_intelligence/proto/ranker_example.pb.h"

class GURL;

namespace base {
class FilePath;
}

namespace net {
class URLRequestContextGetter;
}

namespace machine_intelligence {

class GenericLogisticRegressionInference;

// Predictor class for models that output a binary decision.
class BinaryClassifierPredictor : public BasePredictor {
 public:
  ~BinaryClassifierPredictor() override;

  static std::unique_ptr<BinaryClassifierPredictor> Create(
      net::URLRequestContextGetter* request_context_getter,
      const base::FilePath& model_path,
      GURL model_url,
      const std::string& uma_prefix);

  // Fills in a boolean decision given a RankerExample. Returns false if a
  // prediction could not be made (e.g. the model is not loaded yet).
  bool Predict(const RankerExample& example,
               bool* prediction) WARN_UNUSED_RESULT;

  // Returns a score between 0 and 1. Returns false if a
  // prediction could not be made (e.g. the model is not loaded yet).
  bool PredictScore(const RankerExample& example,
                    float* prediction) WARN_UNUSED_RESULT;

  // Validates that the loaded RankerModel is a valid BinaryClassifier model.
  static RankerModelStatus ValidateModel(const RankerModel& model);

 protected:
  // Instatiates the inference module.
  bool Initialize() override;

 private:
  friend class BinaryClassifierPredictorTest;
  BinaryClassifierPredictor();

  // TODO(hamelphi): Use an abstract BinaryClassifierInferenceModule in order to
  // generalize to other models.
  std::unique_ptr<GenericLogisticRegressionInference> inference_module_;

  DISALLOW_COPY_AND_ASSIGN(BinaryClassifierPredictor);
};

}  // namespace machine_intelligence
#endif  // COMPONENTS_MACHINE_INTELLIGENCE_BINARY_CLASSIFIER_PREDICTOR_H_
