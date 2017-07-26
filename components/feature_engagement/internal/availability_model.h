// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_AVAILABILITY_MODEL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_AVAILABILITY_MODEL_H_

#include <stdint.h>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"

namespace base {
struct Feature;
}  // namespace base

namespace feature_engagement {

// An AvailabilityModel tracks when each feature was made available to an
// end user.
class AvailabilityModel {
 public:
  // Invoked when the availability data has finished loading, and whether the
  // load was a success. In the case of a failure, it is invalid to ever call
  // GetAvailability(...).
  using OnInitializedCallback = base::OnceCallback<void(bool success)>;

  virtual ~AvailabilityModel() = default;

  // Starts initialization of the AvailabilityModel.
  virtual void Initialize(OnInitializedCallback callback,
                          uint32_t current_day) = 0;

  // Returns whether the model is ready, i.e. whether it has been successfully
  // initialized.
  virtual bool IsReady() const = 0;

  // Returns the day number since epoch (1970-01-01) in the local timezone for
  // when the particular |feature| was made available.
  // See TimeProvider::GetCurrentDay().
  virtual base::Optional<uint32_t> GetAvailability(
      const base::Feature& feature) const = 0;

 protected:
  AvailabilityModel() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AvailabilityModel);
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_AVAILABILITY_MODEL_H_
