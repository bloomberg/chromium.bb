// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/never_availability_model.h"

#include "base/callback.h"
#include "base/optional.h"

namespace feature_engagement_tracker {

NeverAvailabilityModel::NeverAvailabilityModel() = default;

NeverAvailabilityModel::~NeverAvailabilityModel() = default;

void NeverAvailabilityModel::Initialize(OnInitializedCallback callback,
                                        uint32_t current_day) {}

bool NeverAvailabilityModel::IsReady() const {
  return false;
}

base::Optional<uint32_t> NeverAvailabilityModel::GetAvailability(
    const base::Feature& feature) const {
  return base::nullopt;
}

}  // namespace feature_engagement_tracker
