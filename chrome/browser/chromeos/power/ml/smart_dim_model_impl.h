// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/power/ml/smart_dim_model.h"

namespace chromeos {
namespace power {
namespace ml {

// Real implementation of SmartDimModel that predicts whether an upcoming screen
// dim should go ahead based on user activity/inactivity following dim.
class SmartDimModelImpl : public SmartDimModel {
 public:
  SmartDimModelImpl();
  ~SmartDimModelImpl() override;

  // chromeos::power::ml::SmartDimModel overrides:
  bool ShouldDim(const UserActivityEvent::Features& features,
                 float* inactive_probability_out,
                 float* threshold_out) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SmartDimModelImpl);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_IMPL_H_
