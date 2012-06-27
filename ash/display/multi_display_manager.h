// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_MULTI_DISPLAY_MANAGER_H_
#define ASH_DISPLAY_MULTI_DISPLAY_MANAGER_H_
#pragma once

#include <vector>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "ui/aura/display_manager.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window.h"

namespace gfx {
class Insets;
class Display;
}

namespace ash {
namespace internal {

// MultiDisplayManager maintains the current display configurations,
// and notifies observers when configuration changes.
// This is exported for unittest.
//
// TODO(oshima): gfx::Screen needs to return translated coordinates
// if the root window is translated. crbug.com/119268.
class ASH_EXPORT MultiDisplayManager : public aura::DisplayManager,
                                       public aura::RootWindowObserver {
 public:
  MultiDisplayManager();
  virtual ~MultiDisplayManager();

  // Used to emulate display change when run in a desktop environment instead
  // of on a device.
  static void AddRemoveDisplay();
  static void CycleDisplay();
  static void ToggleDisplayScale();

  bool UpdateWorkAreaOfDisplayNearestWindow(const aura::Window* window,
                                            const gfx::Insets& insets);

  // DisplayManager overrides:
  virtual void OnNativeDisplaysChanged(
      const std::vector<gfx::Display>& displays) OVERRIDE;
  virtual aura::RootWindow* CreateRootWindowForDisplay(
      const gfx::Display& display) OVERRIDE;
  virtual gfx::Display* GetDisplayAt(size_t index) OVERRIDE;

  virtual size_t GetNumDisplays() const OVERRIDE;
  virtual const gfx::Display& GetDisplayNearestPoint(
      const gfx::Point& point) const OVERRIDE;
  virtual const gfx::Display& GetDisplayNearestWindow(
      const aura::Window* window) const OVERRIDE;

  // RootWindowObserver overrides:
  virtual void OnRootWindowResized(const aura::RootWindow* root,
                                   const gfx::Size& new_size) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtendedDesktopTest, ConvertPoint);
  typedef std::vector<gfx::Display> Displays;

  void Init();
  void AddRemoveDisplayImpl();
  void CycleDisplayImpl();
  void ScaleDisplayImpl();
  gfx::Display& FindDisplayForRootWindow(const aura::RootWindow* root);

  Displays displays_;

  DISALLOW_COPY_AND_ASSIGN(MultiDisplayManager);
};

extern const aura::WindowProperty<int>* const kDisplayIdKey;

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_MULTI_DISPLAY_MANAGER_H_
