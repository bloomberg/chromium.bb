// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a mock location provider and the factory functions for
// various ways of creating it.

#include "device/geolocation/mock_location_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"

namespace device {

MockLocationProvider::MockLocationProvider()
    : state_(STOPPED),
      is_permission_granted_(false),
      provider_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

MockLocationProvider::~MockLocationProvider() {}

void MockLocationProvider::HandlePositionChanged(const Geoposition& position) {
  if (provider_task_runner_->BelongsToCurrentThread()) {
    // The location arbitrator unit tests rely on this method running
    // synchronously.
    position_ = position;
    NotifyCallback(position_);
  } else {
    provider_task_runner_->PostTask(
        FROM_HERE, base::Bind(&MockLocationProvider::HandlePositionChanged,
                              base::Unretained(this), position));
  }
}

bool MockLocationProvider::StartProvider(bool high_accuracy) {
  state_ = high_accuracy ? HIGH_ACCURACY : LOW_ACCURACY;
  return true;
}

void MockLocationProvider::StopProvider() {
  state_ = STOPPED;
}

const Geoposition& MockLocationProvider::GetPosition() {
  return position_;
}

void MockLocationProvider::OnPermissionGranted() {
  is_permission_granted_ = true;
}

}  // namespace device
