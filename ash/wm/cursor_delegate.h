// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CURSOR_DELEGATE_H_
#define ASH_WM_CURSOR_DELEGATE_H_

#include "ash/ash_export.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {

// This interface is implmented by a platform specific object that changes
// the cursor's image and visibility.
class ASH_EXPORT CursorDelegate {
 public:
  virtual void SetCursor(gfx::NativeCursor cursor) = 0;
  virtual void ShowCursor(bool visible) = 0;

 protected:
  virtual ~CursorDelegate() {};
};

}  // namespace aura

#endif  // ASH_WM_CURSOR_DELEGATE_H_
