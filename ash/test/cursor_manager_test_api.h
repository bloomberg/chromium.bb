// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_CURSOR_MANAGER_TEST_API_H_
#define ASH_TEST_CURSOR_MANAGER_TEST_API_H_

#include "base/basictypes.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/display.h"
#include "ui/gfx/native_widget_types.h"

namespace wm {
class CursorManager;
}

namespace ash {
namespace test {

// Use the api in this class to test CursorManager.
class CursorManagerTestApi {
 public:
  explicit CursorManagerTestApi(::wm::CursorManager* cursor_manager);
  ~CursorManagerTestApi();

  ui::CursorSetType GetCurrentCursorSet() const;
  gfx::NativeCursor GetCurrentCursor() const;
  gfx::Display::Rotation GetCurrentCursorRotation() const;
  float GetCurrentCursorScale() const;

 private:
  ::wm::CursorManager* cursor_manager_;

  DISALLOW_COPY_AND_ASSIGN(CursorManagerTestApi);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_CURSOR_MANAGER_TEST_API_H_
