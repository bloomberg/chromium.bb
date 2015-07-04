// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_registration_options.h"

namespace content {

// TODO(thakis): Remove this once http://crbug.com/488634 is fixed.
BackgroundSyncRegistrationOptions::BackgroundSyncRegistrationOptions() =
    default;

bool BackgroundSyncRegistrationOptions::Equals(
    const BackgroundSyncRegistrationOptions& other) const {
  return tag == other.tag && min_period == other.min_period &&
         network_state == other.network_state &&
         power_state == other.power_state && periodicity == other.periodicity;
}

}  // namespace content
