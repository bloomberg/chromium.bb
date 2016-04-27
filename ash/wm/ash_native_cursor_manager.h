// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ASH_NATIVE_CURSOR_MANAGER_H_
#define ASH_WM_ASH_NATIVE_CURSOR_MANAGER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/wm/core/native_cursor_manager.h"
#include "ui/wm/core/native_cursor_manager_delegate.h"

namespace ui {
class ImageCursors;
}

namespace ash {

namespace test {
class CursorManagerTestApi;
}

// This does the ash-specific setting of cursor details like cursor
// visibility. It communicates back with the CursorManager through the
// NativeCursorManagerDelegate interface, which receives messages about what
// changes were acted on.
class ASH_EXPORT AshNativeCursorManager
    : public ::wm::NativeCursorManager {
 public:
  AshNativeCursorManager();
  ~AshNativeCursorManager() override;

  // Toggle native cursor enabled/disabled.
  // The native cursor is enabled by default. When disabled, we hide the native
  // cursor regardless of visibility state, and let CursorWindowManager draw
  // the cursor.
  void SetNativeCursorEnabled(bool enabled);

 private:
  friend class test::CursorManagerTestApi;

  // Overridden from ::wm::NativeCursorManager:
  void SetDisplay(const display::Display& display,
                  ::wm::NativeCursorManagerDelegate* delegate) override;
  void SetCursor(gfx::NativeCursor cursor,
                 ::wm::NativeCursorManagerDelegate* delegate) override;
  void SetVisibility(bool visible,
                     ::wm::NativeCursorManagerDelegate* delegate) override;
  void SetCursorSet(ui::CursorSetType cursor_set,
                    ::wm::NativeCursorManagerDelegate* delegate) override;
  void SetMouseEventsEnabled(
      bool enabled,
      ::wm::NativeCursorManagerDelegate* delegate) override;

  // The cursor location where the cursor was disabled.
  gfx::Point disabled_cursor_location_;

  bool native_cursor_enabled_;

  std::unique_ptr<ui::ImageCursors> image_cursors_;

  DISALLOW_COPY_AND_ASSIGN(AshNativeCursorManager);
};

}  // namespace ash

#endif  // ASH_WM_ASH_NATIVE_CURSOR_MANAGER_H_
