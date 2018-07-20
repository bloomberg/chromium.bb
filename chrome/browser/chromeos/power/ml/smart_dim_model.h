// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_H_

#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"

namespace chromeos {
namespace power {
namespace ml {

// Interface to indicate whether an upcoming screen dim should go ahead based on
// whether user will remain inactive if screen is dimmed now.
class SmartDimModel {
 public:
  virtual ~SmartDimModel() = default;

  // Returns whether an upcoming dim should go ahead based on input |features|.
  // If |inactive_probability_out| and |threshold_out| are non-null, also
  // returns model confidence (probability that user will remain inactive if
  // screen is dimmed now) and threshold: if probability >= threshold then model
  // will return true for this function. Both |inactive_probability_out| and
  // |threshold_out| are expected to be in the range of [0, 1.0] so that they
  // can be logged as model results.
  virtual bool ShouldDim(const UserActivityEvent::Features& features,
                         float* inactive_probability_out,
                         float* threshold_out) = 0;
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_H_
