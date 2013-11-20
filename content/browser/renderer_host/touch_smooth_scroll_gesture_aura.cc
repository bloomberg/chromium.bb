// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/touch_smooth_scroll_gesture_aura.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/transform.h"

namespace {

void InjectTouchEvent(const gfx::Point& location,
                      ui::EventType type,
                      aura::Window* window) {
  gfx::Point screen_location = location;
  aura::Window* root_window = window->GetRootWindow();
  // First convert the location from Window to RootWindow.
  aura::Window::ConvertPointToTarget(window, root_window, &screen_location);
  // Then convert the location from RootWindow to screen.
  aura::WindowEventDispatcher* dispatcher = root_window->GetDispatcher();
  dispatcher->ConvertPointToHost(&screen_location);
  ui::TouchEvent touch(type, screen_location, 0, 0, ui::EventTimeForNow(),
                       1.0f, 1.0f, 1.0f, 1.0f);
  dispatcher->AsRootWindowHostDelegate()->OnHostTouchEvent(&touch);
}

}  // namespace

namespace content {

TouchSmoothScrollGestureAura::TouchSmoothScrollGestureAura(bool scroll_down,
                                                           int pixels_to_scroll,
                                                           int mouse_event_x,
                                                           int mouse_event_y,
                                                           aura::Window* window)
    : scroll_down_(scroll_down),
      pixels_to_scroll_(pixels_to_scroll),
      pixels_scrolled_(0),
      location_(mouse_event_x, mouse_event_y),
      window_(window) {
}

TouchSmoothScrollGestureAura::~TouchSmoothScrollGestureAura() {}

bool TouchSmoothScrollGestureAura::ForwardInputEvents(
    base::TimeTicks now,
    RenderWidgetHost* host) {
  if (pixels_scrolled_ >= pixels_to_scroll_)
    return false;

  RenderWidgetHostImpl* host_impl = RenderWidgetHostImpl::From(host);
  float position_delta = synthetic_gesture_calculator_.GetDelta(now,
      host_impl->GetSyntheticGestureMessageInterval());

  if (pixels_scrolled_ == 0) {
    InjectTouchEvent(location_, ui::ET_TOUCH_PRESSED, window_);
  }

  location_.Offset(0, scroll_down_ ? -position_delta : position_delta);
    InjectTouchEvent(location_, ui::ET_TOUCH_MOVED, window_);

  pixels_scrolled_ += abs(position_delta);

  if (pixels_scrolled_ >= pixels_to_scroll_) {
    InjectTouchEvent(location_, ui::ET_TOUCH_RELEASED, window_);
  }

  return true;
}

}  // namespace content
