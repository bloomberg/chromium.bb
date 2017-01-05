// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/wm_window_aura_test_api.h"

namespace ash {

// static
int WmWindowAuraTestApi::GlobalMinimumSizeLock::instance_count_ = 0;

WmWindowAuraTestApi::GlobalMinimumSizeLock::GlobalMinimumSizeLock() {
  if (instance_count_ == 0)
    WmWindowAuraTestApi::SetDefaultUseEmptyMinimumSizeForTesting(true);
  instance_count_++;
}

WmWindowAuraTestApi::GlobalMinimumSizeLock::~GlobalMinimumSizeLock() {
  DCHECK_GT(instance_count_, 0);
  instance_count_--;
  if (instance_count_ == 0)
    WmWindowAuraTestApi::SetDefaultUseEmptyMinimumSizeForTesting(false);
}

// static
void WmWindowAuraTestApi::SetDefaultUseEmptyMinimumSizeForTesting(bool value) {
  WmWindowAura::default_use_empty_minimum_size_for_testing_ = value;
}

}  // namespace ash
