// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCREEN_ASH_H_
#define ASH_SCREEN_ASH_H_
#pragma once

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen_impl.h"

namespace ash {

// Aura implementation of gfx::Screen. Implemented here to avoid circular
// dependencies.
class ASH_EXPORT ScreenAsh : public gfx::ScreenImpl {
 public:
  ScreenAsh();
  virtual ~ScreenAsh();

  // Returns the bounds for maximized windows. Maximized windows trigger
  // auto-hiding the shelf.
  static gfx::Rect GetMaximizedWindowBounds(aura::Window* window);

  // Returns work area when a maximized window is not present.
  static gfx::Rect GetUnmaximizedWorkAreaBounds(aura::Window* window);

 protected:
  virtual gfx::Point GetCursorScreenPoint() OVERRIDE;
  virtual gfx::NativeWindow GetWindowAtCursorScreenPoint() OVERRIDE;

  virtual int GetNumDisplays() OVERRIDE;
  virtual gfx::Display GetDisplayNearestWindow(
      gfx::NativeView view) const OVERRIDE;
  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const OVERRIDE;
  virtual gfx::Display GetPrimaryDisplay() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenAsh);
};

}  // namespace ash

#endif  // ASH_SCREEN_ASH_H_
