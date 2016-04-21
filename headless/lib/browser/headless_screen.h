// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace gfx {
class Insets;
class Rect;
class Transform;
}

namespace aura {
class Window;
class WindowTreeClient;
class WindowTreeHost;
}

namespace headless {

class HeadlessScreen : public gfx::Screen, public aura::WindowObserver {
 public:
  // Creates a gfx::Screen of the specified size. If no size is specified, then
  // creates a 800x600 screen. |size| is in physical pixels.
  static HeadlessScreen* Create(const gfx::Size& size);
  ~HeadlessScreen() override;

  aura::WindowTreeHost* CreateHostForPrimaryDisplay();

  void SetDeviceScaleFactor(float device_scale_fator);
  void SetDisplayRotation(gfx::Display::Rotation rotation);
  void SetUIScale(float ui_scale);
  void SetWorkAreaInsets(const gfx::Insets& insets);

 protected:
  gfx::Transform GetRotationTransform() const;
  gfx::Transform GetUIScaleTransform() const;

  // WindowObserver overrides:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroying(aura::Window* window) override;

  // gfx::Screen overrides:
  gfx::Point GetCursorScreenPoint() override;
  gfx::NativeWindow GetWindowUnderCursor() override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  int GetNumDisplays() const override;
  std::vector<gfx::Display> GetAllDisplays() const override;
  gfx::Display GetDisplayNearestWindow(gfx::NativeView view) const override;
  gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const override;
  gfx::Display GetDisplayMatching(const gfx::Rect& match_rect) const override;
  gfx::Display GetPrimaryDisplay() const override;
  void AddObserver(gfx::DisplayObserver* observer) override;
  void RemoveObserver(gfx::DisplayObserver* observer) override;

 private:
  explicit HeadlessScreen(const gfx::Rect& screen_bounds);

  aura::WindowTreeHost* host_;
  gfx::Display display_;
  float ui_scale_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessScreen);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_H_
