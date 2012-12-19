// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/cursor_manager_test_api.h"

#include "ash/wm/cursor_manager.h"
#include "ash/wm/image_cursors.h"

namespace ash {
namespace test {

CursorManagerTestApi::CursorManagerTestApi(CursorManager* cursor_manager)
    : cursor_manager_(cursor_manager) {
}

CursorManagerTestApi::~CursorManagerTestApi() {
}

gfx::NativeCursor CursorManagerTestApi::GetCurrentCursor() const {
  return cursor_manager_->GetCurrentCursor();
}

float CursorManagerTestApi::GetDeviceScaleFactor() const {
  return cursor_manager_->image_cursors_->GetDeviceScaleFactor();
}

}  // namespace test
}  // namespace ash
