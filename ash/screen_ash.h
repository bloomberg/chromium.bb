// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCREEN_ASH_H_
#define ASH_SCREEN_ASH_H_
#pragma once

#include "base/compiler_specific.h"
#include "ash/ash_export.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"

namespace aura {
class RootWindow;
}

namespace ash {

// Aura implementation of gfx::Screen. Implemented here to avoid circular
// dependencies.
class ASH_EXPORT ScreenAsh : public gfx::Screen {
 public:
  explicit ScreenAsh(aura::RootWindow* root_window);
  virtual ~ScreenAsh();

 protected:
  virtual gfx::Point GetCursorScreenPointImpl() OVERRIDE;
  virtual gfx::Rect GetMonitorWorkAreaNearestWindowImpl(
      gfx::NativeView view) OVERRIDE;
  virtual gfx::Rect GetMonitorAreaNearestWindowImpl(
      gfx::NativeView view) OVERRIDE;
  virtual gfx::Rect GetMonitorWorkAreaNearestPointImpl(
      const gfx::Point& point) OVERRIDE;
  virtual gfx::Rect GetMonitorAreaNearestPointImpl(
      const gfx::Point& point) OVERRIDE;
  virtual gfx::NativeWindow GetWindowAtCursorScreenPointImpl() OVERRIDE;
  virtual gfx::Size GetPrimaryMonitorSizeImpl() OVERRIDE;
  virtual int GetNumMonitorsImpl() OVERRIDE;

 private:
  aura::RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(ScreenAsh);
};

}  // namespace ash

#endif  // ASH_SCREEN_ASH_H_
