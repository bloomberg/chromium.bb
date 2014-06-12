// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/panels/x11_panel_resizer.h"

#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/views/view.h"

namespace {

// Returns true if the window can be resized via |component|.
bool IsWindowBorder(int component) {
  return component == HTBOTTOM ||
      component == HTBOTTOMLEFT ||
      component == HTBOTTOMRIGHT ||
      component == HTLEFT ||
      component == HTRIGHT ||
      component == HTTOP ||
      component == HTTOPLEFT ||
      component == HTTOPRIGHT;
}

}  // namespace

X11PanelResizer::X11PanelResizer(Panel* panel, aura::Window* window)
    : panel_(panel),
      window_(window),
      resize_state_(NOT_RESIZING),
      resize_component_(HTNOWHERE) {
}

X11PanelResizer::~X11PanelResizer() {
  StopResizing(NULL, true);
}

void X11PanelResizer::OnMousePressed(ui::MouseEvent* event) {
  if (resize_state_ != NOT_RESIZING ||
      event->type() != ui::ET_MOUSE_PRESSED ||
      !event->IsLeftMouseButton() ||
      !event->native_event()) {
    return;
  }

  int component = window_->delegate()->GetNonClientComponent(event->location());
  if (!IsWindowBorder(component))
    return;

  // Set capture so that we get notified of all subsequent events.
  window_->SetCapture();

  resize_state_ = RESIZE_CAN_START;
  initial_press_location_in_screen_ = ui::EventSystemLocationFromNative(
      event->native_event());
  resize_component_ = component;
  event->StopPropagation();
}

void X11PanelResizer::OnMouseDragged(ui::MouseEvent* event) {
  if (resize_state_ != RESIZE_CAN_START &&
      resize_state_ != RESIZE_IN_PROGRESS) {
    return;
  }

  if (!event->native_event())
    return;

  // Get the location in screen coordinates from the XEvent because converting
  // the mouse location to screen coordinates using ScreenPositionClient returns
  // an incorrect location while the panel is moving. See crbug.com/353393 for
  // more details.
  // TODO: Fix conversion to screen coordinates.
  gfx::Point location_in_screen = ui::EventSystemLocationFromNative(
      event->native_event());
  if (resize_state_ == RESIZE_CAN_START) {
    gfx::Vector2d delta =
        location_in_screen - initial_press_location_in_screen_;
    if (views::View::ExceededDragThreshold(delta)) {
      resize_state_ = RESIZE_IN_PROGRESS;
      panel_->manager()->StartResizingByMouse(panel_, location_in_screen,
                                              resize_component_);
    }
  }

  if (resize_state_ == RESIZE_IN_PROGRESS)
    panel_->manager()->ResizeByMouse(location_in_screen);

  event->StopPropagation();
}

void X11PanelResizer::StopResizing(ui::MouseEvent* event, bool canceled) {
  if (resize_state_ == NOT_RESIZING)
    return;

  if (resize_state_ == RESIZE_IN_PROGRESS) {
    panel_->manager()->EndResizingByMouse(canceled);
    window_->ReleaseCapture();
  }
  if (event)
    event->StopPropagation();
  resize_state_ = NOT_RESIZING;
}

void X11PanelResizer::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      OnMousePressed(event);
      break;
    case ui::ET_MOUSE_DRAGGED:
      OnMouseDragged(event);
      break;
    case ui::ET_MOUSE_RELEASED:
      StopResizing(event, false);
      break;
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      StopResizing(event, true);
      break;
    default:
      break;
  }
}
