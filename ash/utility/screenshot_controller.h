// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_SCREENSHOT_CONTROLLER_H_
#define ASH_UTILITY_SCREENSHOT_CONTROLLER_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"

namespace aura {
class Window;
}

namespace ui {
class LocatedEvent;
}

namespace ash {
class ScreenshotDelegate;

// This class controls a session of taking partial/window screenshot, i.e.:
// drawing
// region rectangles during selection, and changing the mouse cursor to indicate
// the current mode.
// This class does not use aura::Window / views::Widget intentionally to avoid
class ASH_EXPORT ScreenshotController : public ui::EventHandler,
                                        public display::DisplayObserver,
                                        public aura::WindowObserver {
 public:
  ScreenshotController();
  ~ScreenshotController() override;

  // Starts the UI for taking partial screenshot; dragging to select a region.
  // ScreenshotController manage their own lifetime so caller must not
  // delete the returned values.
  void StartPartialScreenshotSession(ScreenshotDelegate* screenshot_delegate);

  // Starts the UI for taking a window screenshot;
  void StartWindowScreenshotSession(ScreenshotDelegate* screenshot_delegate);

 private:
  enum Mode {
    NONE,
    PARTIAL,
    WINDOW,
  };

  friend class ScreenshotControllerTest;

  class ScopedCursorSetter;
  class ScreenshotLayer;

  // Starts, ends, cancels, or updates the region selection.
  void MaybeStart(const ui::LocatedEvent& event);
  void CompleteWindowScreenshot();
  void CompletePartialScreenshot();
  void Cancel();
  void Update(const ui::LocatedEvent& event);
  void UpdateSelectedWindow(ui::LocatedEvent* event);
  void SetSelectedWindow(aura::Window* window);

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  Mode mode_;

  // The data to build the screenshot region.
  gfx::Point start_position_;
  aura::Window* root_window_;

  // Currently selected window in WINDOW mode.
  aura::Window* selected_;

  // Layers to create the visual effect of region selection or selected window.
  std::map<aura::Window*, ScreenshotLayer*> layers_;

  // The object to specify the crosshair cursor.
  std::unique_ptr<ScopedCursorSetter> cursor_setter_;

  // ScreenshotDelegate to take the actual screenshot. No ownership.
  ScreenshotDelegate* screenshot_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotController);
};

}  // namespace ash

#endif  // ASH_UTILITY_SCREENSHOT_CONTROLLER_H_
