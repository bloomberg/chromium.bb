// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PANELS_X11_PANEL_RESIZER_H_
#define CHROME_BROWSER_UI_VIEWS_PANELS_X11_PANEL_RESIZER_H_

#include "ui/events/event_handler.h"
#include "ui/gfx/point.h"

namespace aura {
class Window;
}

class Panel;

// EventHandler which implements special handling for resizing panels. Panels
// should use X11PanelResizer instead of X11WindowEventFilter.
// When resizing a panel:
// - The panel bounds should update in sync with the user's mouse movement
//   during the resize operation.
// - The layout of all of the panels should be updated once the resize operation
//   completes.
// X11PanelResizer does not hand off resizing to the window manager because it
// is not possible to determine when the window manager has finished resizing.
class X11PanelResizer : public ui::EventHandler {
 public:
  X11PanelResizer(Panel* panel, aura::Window* window);
  virtual ~X11PanelResizer();

 private:
  enum ResizeState {
    NOT_RESIZING,

    // The user has clicked on one of the panel's edges.
    RESIZE_CAN_START,

    // The user has dragged one of the panel's edges far enough for resizing to
    // begin.
    RESIZE_IN_PROGRESS
  };

  // Called as a result of ET_MOUSE_PRESSED.
  void OnMousePressed(ui::MouseEvent* event);

  // Called as a result of ET_MOUSE_DRAGGED.
  void OnMouseDragged(ui::MouseEvent* event);

  // Stops the resize operation if one is in progress. Consumes |event| if it
  // is not NULL and the resize operation was stopped.
  void StopResizing(ui::MouseEvent* event, bool canceled);

  // ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;

  // The panel being resized.
  Panel* panel_;

  // |panel|'s window.
  aura::Window* window_;

  ResizeState resize_state_;

  // The edge that the user is resizing.
  int resize_component_;

  // The location of the mouse click on the window border.
  gfx::Point initial_press_location_in_screen_;

  DISALLOW_COPY_AND_ASSIGN(X11PanelResizer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PANELS_X11_PANEL_RESIZER_H_
