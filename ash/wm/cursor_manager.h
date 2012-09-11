// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CURSOR_MANAGER_H_
#define ASH_WM_CURSOR_MANAGER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {
class CursorDelegate;
class ImageCursors;

// This class controls the visibility and the type of the cursor.
// The cursor type can be locked so that the type stays the same
// until it's unlocked.
class CursorManager : public aura::client::CursorClient {
 public:
  CursorManager();
  virtual ~CursorManager();

  void set_delegate(CursorDelegate* delegate) { delegate_ = delegate; }

  // Locks/Unlocks the cursor change.
  void LockCursor();
  void UnlockCursor();

  bool is_cursor_locked() const { return cursor_lock_count_ > 0; }

  // Shows or hides the cursor.
  bool cursor_visible() const { return cursor_visible_; }

  // Overridden from aura::client::CursorClient:
  virtual void SetCursor(gfx::NativeCursor) OVERRIDE;
  virtual void ShowCursor(bool show) OVERRIDE;
  virtual bool IsCursorVisible() const OVERRIDE;
  virtual void SetDeviceScaleFactor(float device_scale_factor) OVERRIDE;

 private:
  void SetCursorInternal(gfx::NativeCursor cursor);

  CursorDelegate* delegate_;

  // Number of times LockCursor() has been invoked without a corresponding
  // UnlockCursor().
  int cursor_lock_count_;

  // Set to true if UpdateCursor() is invoked while |cursor_lock_count_| == 0.
  bool did_cursor_change_;

  // Cursor to set once |cursor_lock_count_| is set to 0. Only valid if
  // |did_cursor_change_| is true.
  gfx::NativeCursor cursor_to_set_on_unlock_;

  // Is cursor visible?
  bool cursor_visible_;

  // The cursor currently set.
  gfx::NativeCursor current_cursor_;

  scoped_ptr<ImageCursors> image_cursors_;

  DISALLOW_COPY_AND_ASSIGN(CursorManager);
};

}  // namespace aura

#endif  // UI_AURA_CURSOR_MANAGER_H_
