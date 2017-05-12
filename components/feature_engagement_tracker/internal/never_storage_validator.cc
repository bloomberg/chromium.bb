// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/never_storage_validator.h"

namespace feature_engagement_tracker {

NeverStorageValidator::NeverStorageValidator() = default;

NeverStorageValidator::~NeverStorageValidator() = default;

bool NeverStorageValidator::ShouldStore(const std::string& event_name) const {
  return false;
}

bool NeverStorageValidator::ShouldKeep(const std::string& event_name,
                                       uint32_t event_day,
                                       uint32_t current_day) const {
  return false;
}

}  // namespace feature_engagement_tracker
