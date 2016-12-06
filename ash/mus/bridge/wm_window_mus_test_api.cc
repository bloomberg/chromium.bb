// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_window_mus_test_api.h"

namespace ash {
namespace mus {

// static
int WmWindowMusTestApi::GlobalMinimumSizeLock::instance_count_ = 0;

WmWindowMusTestApi::GlobalMinimumSizeLock::GlobalMinimumSizeLock() {
  if (instance_count_ == 0)
    WmWindowMusTestApi::SetDefaultUseEmptyMinimumSizeForTesting(true);
  instance_count_++;
}

WmWindowMusTestApi::GlobalMinimumSizeLock::~GlobalMinimumSizeLock() {
  DCHECK_GT(instance_count_, 0);
  instance_count_--;
  if (instance_count_ == 0)
    WmWindowMusTestApi::SetDefaultUseEmptyMinimumSizeForTesting(false);
}

// static
void WmWindowMusTestApi::SetDefaultUseEmptyMinimumSizeForTesting(bool value) {
  WmWindowMus::default_use_empty_minimum_size_for_testing_ = value;
}

}  // namespace mus
}  // namespace ash
