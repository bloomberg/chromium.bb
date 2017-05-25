// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_NEVER_AVAILABILITY_MODEL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_NEVER_AVAILABILITY_MODEL_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/feature_engagement_tracker/internal/availability_model.h"

namespace feature_engagement_tracker {

// An AvailabilityModel that never has any data, and is never ready.
class NeverAvailabilityModel : public AvailabilityModel {
 public:
  NeverAvailabilityModel();
  ~NeverAvailabilityModel() override;

  // AvailabilityModel implementation.
  void Initialize(AvailabilityModel::OnInitializedCallback callback,
                  uint32_t current_day) override;
  bool IsReady() const override;
  base::Optional<uint32_t> GetAvailability(
      const base::Feature& feature) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NeverAvailabilityModel);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_NEVER_AVAILABILITY_MODEL_H_
