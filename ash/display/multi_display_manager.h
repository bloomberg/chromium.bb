// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_MULTI_DISPLAY_MANAGER_H_
#define ASH_DISPLAY_MULTI_DISPLAY_MANAGER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "ui/aura/display_manager.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window.h"

namespace gfx {
class Display;
class Insets;
class Rect;
}

namespace ash {
namespace test {
class AcceleratorControllerTest;
class SystemGestureEventFilterTest;
}
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
  static void CycleDisplay();
  static void ToggleDisplayScale();

  // Detects the internal display's ID, and stores gfx::Display
  // in the cache, if any.
  void InitInternalDisplayInfo();

  // True if there is an internal display.
  bool HasInternalDisplay() const;

  bool UpdateWorkAreaOfDisplayNearestWindow(const aura::Window* window,
                                            const gfx::Insets& insets);

  // Finds the display that contains |point| in screeen coordinates.
  // Returns invalid display if there is no display that can satisfy
  // the condition.
  const gfx::Display& FindDisplayContainingPoint(
      const gfx::Point& point_in_screen) const;

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
  virtual const gfx::Display& GetDisplayMatching(
      const gfx::Rect& match_rect)const OVERRIDE;
  virtual std::string GetDisplayNameAt(size_t index) OVERRIDE;

  // RootWindowObserver overrides:
  virtual void OnRootWindowResized(const aura::RootWindow* root,
                                   const gfx::Size& new_size) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtendedDesktopTest, ConvertPoint);
  FRIEND_TEST_ALL_PREFIXES(MultiDisplayManagerTest, TestNativeDisplaysChanged);
  friend class test::AcceleratorControllerTest;
  friend class test::SystemGestureEventFilterTest;

  typedef std::vector<gfx::Display> Displays;

  void Init();
  void CycleDisplayImpl();
  void ScaleDisplayImpl();
  gfx::Display& FindDisplayForRootWindow(const aura::RootWindow* root);

  // Refer to |aura::DisplayManager::CreateDisplayFromSpec| API for
  // the format of |spec|.
  void AddDisplayFromSpec(const std::string& spec);

  // Enables internal display and returns the display Id for the internal
  // display.
  int64 EnableInternalDisplayForTest();

  Displays displays_;

  int64 internal_display_id_;

  // An internal display cache used when the internal display is disconnectd.
  scoped_ptr<gfx::Display> internal_display_;

  DISALLOW_COPY_AND_ASSIGN(MultiDisplayManager);
};

extern const aura::WindowProperty<int64>* const kDisplayIdKey;

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_MULTI_DISPLAY_MANAGER_H_
