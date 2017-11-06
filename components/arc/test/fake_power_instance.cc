// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_power_instance.h"

#include <utility>

namespace arc {

FakePowerInstance::FakePowerInstance() = default;

FakePowerInstance::~FakePowerInstance() = default;

FakePowerInstance::SuspendCallback FakePowerInstance::GetSuspendCallback() {
  return std::move(suspend_callback_);
}

void FakePowerInstance::Init(mojom::PowerHostPtr host_ptr) {
  host_ptr_ = std::move(host_ptr);
}

void FakePowerInstance::SetInteractive(bool enabled) {
  interactive_ = enabled;
}

void FakePowerInstance::Suspend(SuspendCallback callback) {
  num_suspend_++;
  suspend_callback_ = std::move(callback);
}

void FakePowerInstance::Resume() {
  num_resume_++;
}

void FakePowerInstance::UpdateScreenBrightnessSettings(double percent) {
  screen_brightness_ = percent;
}

}  // namespace arc
