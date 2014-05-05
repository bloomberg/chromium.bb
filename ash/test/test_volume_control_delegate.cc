// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_volume_control_delegate.h"

namespace ash {

TestVolumeControlDelegate::TestVolumeControlDelegate(bool consume)
    : consume_(consume),
      handle_volume_mute_count_(0),
      handle_volume_down_count_(0),
      handle_volume_up_count_(0) {
}

TestVolumeControlDelegate::~TestVolumeControlDelegate() {
}

bool TestVolumeControlDelegate::HandleVolumeMute(
    const ui::Accelerator& accelerator) {
  ++handle_volume_mute_count_;
  last_accelerator_ = accelerator;
  return consume_;
}

bool TestVolumeControlDelegate::HandleVolumeDown(
    const ui::Accelerator& accelerator) {
  ++handle_volume_down_count_;
  last_accelerator_ = accelerator;
  return consume_;
}

bool TestVolumeControlDelegate::HandleVolumeUp(
    const ui::Accelerator& accelerator) {
  ++handle_volume_up_count_;
  last_accelerator_ = accelerator;
  return consume_;
}

}  // namespace ash
