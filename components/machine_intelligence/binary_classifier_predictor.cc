// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/machine_intelligence/binary_classifier_predictor.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "components/machine_intelligence/generic_logistic_regression_inference.h"
#include "components/machine_intelligence/proto/ranker_model.pb.h"
#include "components/machine_intelligence/ranker_model.h"
#include "components/machine_intelligence/ranker_model_loader_impl.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace machine_intelligence {

BinaryClassifierPredictor::BinaryClassifierPredictor(){};
BinaryClassifierPredictor::~BinaryClassifierPredictor(){};

// static
std::unique_ptr<BinaryClassifierPredictor> BinaryClassifierPredictor::Create(
    net::URLRequestContextGetter* request_context_getter,
    const base::FilePath& model_path,
    GURL model_url,
    const std::string& uma_prefix) {
  std::unique_ptr<BinaryClassifierPredictor> predictor(
      new BinaryClassifierPredictor());
  auto model_loader = base::MakeUnique<RankerModelLoaderImpl>(
      base::Bind(&BinaryClassifierPredictor::ValidateModel),
      base::Bind(&BinaryClassifierPredictor::OnModelAvailable,
                 base::Unretained(predictor.get())),
      request_context_getter, model_path, model_url, uma_prefix);
  predictor->LoadModel(std::move(model_loader));
  return predictor;
}

bool BinaryClassifierPredictor::Predict(const RankerExample& example,
                                        bool* prediction) {
  if (!IsReady()) {
    return false;
  }
  *prediction = inference_module_->Predict(example);
  return true;
}

bool BinaryClassifierPredictor::PredictScore(const RankerExample& example,
                                             float* prediction) {
  if (!IsReady()) {
    return false;
  }
  *prediction = inference_module_->PredictScore(example);
  return true;
}

// static
RankerModelStatus BinaryClassifierPredictor::ValidateModel(
    const RankerModel& model) {
  if (model.proto().model_case() != RankerModelProto::kLogisticRegression) {
    return RankerModelStatus::INCOMPATIBLE;
  }
  return RankerModelStatus::OK;
}

bool BinaryClassifierPredictor::Initialize() {
  // TODO(hamelphi): move the GLRM proto up one layer in the proto in order to
  // be independent of the client feature.
  inference_module_.reset(new GenericLogisticRegressionInference(
      ranker_model_->proto().logistic_regression()));
  return true;
}

}  // namespace machine_intelligence
