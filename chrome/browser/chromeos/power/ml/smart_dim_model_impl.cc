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
bool SmartDimModelImpl::ShouldDim(const UserActivityEvent::Features& features,
                                  float* inactive_probability_out,
                                  float* threshold_out) {
  // Let dim go ahead before we have a model implementation in place.
  if (inactive_probability_out) {
    *inactive_probability_out = 1.0;
  }
  if (threshold_out) {
    *threshold_out = 0.0;
  }
  return true;
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
