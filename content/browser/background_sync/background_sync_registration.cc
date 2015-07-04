// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_registration.h"

namespace content {

// TODO(thakis): Remove this once http://crbug.com/488634 is fixed.
BackgroundSyncRegistration::BackgroundSyncRegistration() = default;

const BackgroundSyncRegistration::RegistrationId
    BackgroundSyncRegistration::kInvalidRegistrationId = -1;

const BackgroundSyncRegistration::RegistrationId
    BackgroundSyncRegistration::kInitialId = 0;

bool BackgroundSyncRegistration::Equals(
    const BackgroundSyncRegistration& other) const {
  return options_.Equals(other.options_);
}

bool BackgroundSyncRegistration::IsValid() const {
  return id_ != kInvalidRegistrationId;
}

}  // namespace content
