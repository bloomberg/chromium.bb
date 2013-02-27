// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ASH_NATIVE_CURSOR_MANAGER_H_
#define ASH_WM_ASH_NATIVE_CURSOR_MANAGER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/views/corewm/native_cursor_manager.h"
#include "ui/views/corewm/native_cursor_manager_delegate.h"

namespace ash {

namespace test {
class CursorManagerTestApi;
}

class ImageCursors;

// This does the ash-specific setting of cursor details like cursor
// visibility. It communicates back with the CursorManager through the
// NativeCursorManagerDelegate interface, which receives messages about what
// changes were acted on.
class ASH_EXPORT AshNativeCursorManager
    : public views::corewm::NativeCursorManager {
 public:
  AshNativeCursorManager();
  virtual ~AshNativeCursorManager();

 private:
  friend class test::CursorManagerTestApi;

  // Overridden from views::corewm::NativeCursorManager:
  virtual void SetDeviceScaleFactor(
      float device_scale_factor,
      views::corewm::NativeCursorManagerDelegate* delegate) OVERRIDE;
  virtual void SetCursor(
      gfx::NativeCursor cursor,
      views::corewm::NativeCursorManagerDelegate* delegate) OVERRIDE;
  virtual void SetVisibility(
      bool visible,
      views::corewm::NativeCursorManagerDelegate* delegate) OVERRIDE;
  virtual void SetMouseEventsEnabled(
      bool enabled,
      views::corewm::NativeCursorManagerDelegate* delegate) OVERRIDE;

  // The cursor location where the cursor was disabled.
  gfx::Point disabled_cursor_location_;

  scoped_ptr<ImageCursors> image_cursors_;

  DISALLOW_COPY_AND_ASSIGN(AshNativeCursorManager);
};

}  // namespace ash

#endif  // ASH_WM_ASH_NATIVE_CURSOR_MANAGER_H_
