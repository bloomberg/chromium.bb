// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_background_sync_controller.h"

namespace content {

void MockBackgroundSyncController::NotifyBackgroundSyncRegistered(
    const url::Origin& origin) {
  registration_count_ += 1;
  registration_origin_ = origin;
}

void MockBackgroundSyncController::RunInBackground() {
  run_in_background_count_ += 1;
}

void MockBackgroundSyncController::GetParameterOverrides(
    BackgroundSyncParameters* parameters) const {
  *parameters = background_sync_parameters_;
}

}  // namespace content
