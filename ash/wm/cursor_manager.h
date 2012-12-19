// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CURSOR_MANAGER_H_
#define ASH_WM_CURSOR_MANAGER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

namespace ash {

namespace internal {
class CursorState;
}

namespace test {
class CursorManagerTestApi;
}

class ImageCursors;

// This class controls the visibility and the type of the cursor.
// The cursor type can be locked so that the type stays the same
// until it's unlocked.
class ASH_EXPORT CursorManager : public aura::client::CursorClient {
 public:
  CursorManager();
  virtual ~CursorManager();

  bool is_cursor_locked() const { return cursor_lock_count_ > 0; }

  // Overridden from aura::client::CursorClient:
  virtual void SetCursor(gfx::NativeCursor) OVERRIDE;
  virtual void ShowCursor() OVERRIDE;
  virtual void HideCursor() OVERRIDE;
  virtual bool IsCursorVisible() const OVERRIDE;
  virtual void EnableMouseEvents() OVERRIDE;
  virtual void DisableMouseEvents() OVERRIDE;
  virtual bool IsMouseEventsEnabled() const OVERRIDE;
  virtual void SetDeviceScaleFactor(float device_scale_factor) OVERRIDE;
  virtual void LockCursor() OVERRIDE;
  virtual void UnlockCursor() OVERRIDE;

 private:
  friend class test::CursorManagerTestApi;

  void SetCursorInternal(gfx::NativeCursor cursor);
  void SetCursorVisibility(bool visible);
  void SetMouseEventsEnabled(bool enabled);

  // Returns the current cursor.
  gfx::NativeCursor GetCurrentCursor() const;

  // Number of times LockCursor() has been invoked without a corresponding
  // UnlockCursor().
  int cursor_lock_count_;

  // The cursor location where the cursor was disabled.
  gfx::Point disabled_cursor_location_;

  // The current state of the cursor.
  scoped_ptr<internal::CursorState> current_state_;

  // The cursor state to restore when the cursor is unlocked.
  scoped_ptr<internal::CursorState> state_on_unlock_;

  scoped_ptr<ImageCursors> image_cursors_;

  DISALLOW_COPY_AND_ASSIGN(CursorManager);
};

}  // namespace ash

#endif  // UI_AURA_CURSOR_MANAGER_H_
