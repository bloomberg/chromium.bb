// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/machine_intelligence/base_predictor.h"

#include "base/memory/ptr_util.h"
#include "components/machine_intelligence/proto/ranker_model.pb.h"
#include "components/machine_intelligence/ranker_model.h"

namespace machine_intelligence {

BasePredictor::BasePredictor() {}
BasePredictor::~BasePredictor() {}

void BasePredictor::LoadModel(std::unique_ptr<RankerModelLoader> model_loader) {
  if (model_loader_) {
    DLOG(ERROR) << "This predictor already has a model loader.";
    return;
  }
  // Take ownership of the model loader.
  model_loader_ = std::move(model_loader);
  // Kick off the initial load from cache.
  model_loader_->NotifyOfRankerActivity();
}

void BasePredictor::OnModelAvailable(
    std::unique_ptr<machine_intelligence::RankerModel> model) {
  ranker_model_ = std::move(model);
  is_ready_ = Initialize();
}

bool BasePredictor::IsReady() {
  if (!is_ready_)
    model_loader_->NotifyOfRankerActivity();

  return is_ready_;
}

}  // namespace machine_intelligence
