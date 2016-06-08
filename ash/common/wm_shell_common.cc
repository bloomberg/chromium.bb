// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm_shell_common.h"

#include "ash/common/wm/mru_window_tracker.h"

namespace ash {

WmShellCommon::WmShellCommon() {}

WmShellCommon::~WmShellCommon() {}

void WmShellCommon::CreateMruWindowTracker() {
  mru_window_tracker_.reset(new MruWindowTracker);
}

void WmShellCommon::DeleteMruWindowTracker() {
  mru_window_tracker_.reset();
}

}  // namespace ash
