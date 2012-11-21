// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/immersive_mode_controller.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/views/mouse_watcher_view_host.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {

// Time after which the edge trigger fires and top-chrome is revealed.
const int kRevealDelayMs = 200;

// During an immersive-mode top-of-window reveal, how long to wait after the
// mouse exits the views before hiding them again.
const int kHideDelayMs = 200;

}  // namespace

ImmersiveModeController::ImmersiveModeController(BrowserView* browser_view)
    : browser_view_(browser_view),
      enabled_(false),
      hide_top_views_(false) {
}

ImmersiveModeController::~ImmersiveModeController() {}

void ImmersiveModeController::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;
  hide_top_views_ = enabled;
  browser_view_->tabstrip()->SetImmersiveStyle(enabled_);
  browser_view_->Layout();

#if defined(USE_AURA)
  // TODO(jamescook): If we want to port this feature to non-Aura views we'll
  // need a method to monitor incoming mouse move events without handling them.
  // Currently views uses GetEventHandlerForPoint() to route events directly
  // to either a tab or the caption area, bypassing pre-target handlers and
  // intermediate views.
  if (enabled_)
    browser_view_->GetNativeWindow()->AddPreTargetHandler(this);
  else
    browser_view_->GetNativeWindow()->RemovePreTargetHandler(this);
#endif  // defined(USE_AURA)

  if (!enabled_) {
    // Stop watching the mouse on disable.
    mouse_watcher_.reset();
    top_timer_.Stop();
  }
}

void ImmersiveModeController::RevealTopViews() {
  if (!hide_top_views_)
    return;
  hide_top_views_ = false;

  // Recompute the bounds of the views when painted normally.
  browser_view_->tabstrip()->SetImmersiveStyle(false);
  browser_view_->Layout();

  // Compute the top of the content area in order to find the bottom of all
  // the top-of-window views like toolbar, bookmarks, etc.
  gfx::Point content_origin(0, 0);
  views::View::ConvertPointToTarget(
      browser_view_->contents(), browser_view_, &content_origin);

  // Stop the immersive reveal when the mouse leaves the top-of-window area.
  gfx::Insets insets(0, 0, content_origin.y() - browser_view_->height(), 0);
  views::MouseWatcherViewHost* host =
      new views::MouseWatcherViewHost(browser_view_, insets);
  // MouseWatcher takes ownership of |host|.
  mouse_watcher_.reset(new views::MouseWatcher(host, this));
  mouse_watcher_->set_notify_on_exit_time(
      base::TimeDelta::FromMilliseconds(kHideDelayMs));
  mouse_watcher_->Start();
}

// ui::EventHandler overrides:
ui::EventResult ImmersiveModeController::OnKeyEvent(ui::KeyEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::EventResult ImmersiveModeController::OnMouseEvent(ui::MouseEvent* event) {
  if (event->location().y() == 0) {
    // Use a timer to detect if the cursor stays at the top past a delay.
    if (!top_timer_.IsRunning()) {
      top_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(kRevealDelayMs),
                       this, &ImmersiveModeController::RevealTopViews);
    }
  } else {
    // Cursor left the top edge.
    top_timer_.Stop();
  }
  // Pass along event for further handling.
  return ui::ER_UNHANDLED;
}

ui::EventResult ImmersiveModeController::OnScrollEvent(ui::ScrollEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::EventResult ImmersiveModeController::OnTouchEvent(ui::TouchEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::EventResult ImmersiveModeController::OnGestureEvent(
    ui::GestureEvent* event) {
  return ui::ER_UNHANDLED;
}

// views::MouseWatcherListener overrides:
void ImmersiveModeController::MouseMovedOutOfHost() {
  // Stop watching the mouse.
  mouse_watcher_.reset();
  // Stop showing the top views.
  hide_top_views_ = true;
  browser_view_->tabstrip()->SetImmersiveStyle(true);
  browser_view_->Layout();
}
