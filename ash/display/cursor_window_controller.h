// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_CURSOR_WINDOW_CONTROLLER_H_
#define ASH_DISPLAY_CURSOR_WINDOW_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/display/display.h"

namespace ash {
namespace test {
class MirrorWindowTestApi;
}

class CursorWindowControllerTest;
class CursorWindowDelegate;

// Draws a mouse cursor on a given container window.
// When cursor compositing is disabled, CursorWindowController is responsible
// to scale and rotate the mouse cursor bitmap to match settings of the
// primary display.
// When cursor compositing is enabled, just draw the cursor as-is.
class ASH_EXPORT CursorWindowController {
 public:
  CursorWindowController();
  ~CursorWindowController();

  bool is_cursor_compositing_enabled() const {
    return is_cursor_compositing_enabled_;
  }

  void SetLargeCursorSizeInDip(int large_cursor_size_in_dip);

  // Sets cursor compositing mode on/off.
  void SetCursorCompositingEnabled(bool enabled);

  // Updates the container window for the cursor window controller.
  void UpdateContainer();

  // Sets the display on which to draw cursor.
  // Only applicable when cursor compositing is enabled.
  void SetDisplay(const display::Display& display);

  // Sets cursor location, shape, set and visibility.
  void UpdateLocation();
  void SetCursor(gfx::NativeCursor cursor);
  void SetCursorSet(ui::CursorSetType);
  void SetVisibility(bool visible);

 private:
  friend class CursorWindowControllerTest;
  friend class test::MirrorWindowTestApi;

  // Sets the container window for the cursor window controller.
  // Closes the cursor window if |container| is NULL.
  void SetContainer(aura::Window* container);

  // Sets the bounds of the container in screen coordinates.
  void SetBoundsInScreen(const gfx::Rect& bounds);

  // Updates cursor image based on current cursor state.
  void UpdateCursorImage();

  // Hides/shows cursor window based on current cursor state.
  void UpdateCursorVisibility();

  const gfx::ImageSkia& GetCursorImageForTest() const;

  bool is_cursor_compositing_enabled_;
  aura::Window* container_;

  // The bounds of the container in screen coordinates.
  gfx::Rect bounds_in_screen_;

  // The native_type of the cursor, see definitions in cursor.h
  int cursor_type_;

  // The last requested cursor visibility.
  bool visible_;

  ui::CursorSetType cursor_set_;
  gfx::Point hot_point_;

  bool enable_adjustable_large_cursor_;
  int large_cursor_size_in_dip_;

  // The display on which the cursor is drawn.
  // For mirroring mode, the display is always the primary display.
  display::Display display_;

  std::unique_ptr<aura::Window> cursor_window_;
  std::unique_ptr<CursorWindowDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(CursorWindowController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_CURSOR_WINDOW_CONTROLLER_H_
