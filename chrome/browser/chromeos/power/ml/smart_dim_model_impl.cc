// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/smart_dim_model_impl.h"

namespace chromeos {
namespace power {
namespace ml {

SmartDimModelImpl::SmartDimModelImpl() = default;

SmartDimModelImpl::~SmartDimModelImpl() = default;

// TODO(jiameng): add impl.
UserActivityEvent::ModelPrediction SmartDimModelImpl::ShouldDim(
    const UserActivityEvent::Features& features) {
  // Let dim go ahead before we have a model implementation in place.
  UserActivityEvent::ModelPrediction prediction;
  prediction.set_decision_threshold(0);
  prediction.set_inactivity_score(100);
  prediction.set_response(UserActivityEvent::ModelPrediction::DIM);
  return prediction;
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
