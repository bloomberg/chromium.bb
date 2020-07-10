// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_H_

#include "base/callback.h"
#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"

namespace chromeos {
namespace power {
namespace ml {

// Interface to indicate whether an upcoming screen dim should go ahead based on
// whether user will remain inactive if screen is dimmed now.
class SmartDimModel {
 public:
  using DimDecisionCallback =
      base::OnceCallback<void(UserActivityEvent::ModelPrediction)>;
  virtual ~SmartDimModel() = default;

  // Post a request to determine whether an upcoming dim should go ahead based
  // on input |features|. When a decision is arrived at, run the callback
  // specified by |dim_callback|. If this method is called again before it
  // calls the previous callback, the previous callback will be canceled.
  virtual void RequestDimDecision(const UserActivityEvent::Features& features,
                                  DimDecisionCallback dim_callback) = 0;

  // Cancel a previous dim decision request, if one is pending. If no dim
  // decision request is pending, this function has no effect.
  virtual void CancelPreviousRequest() = 0;
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_H_
