// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"

#include "base/bind.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_view.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

namespace {

// +----------------------------+
// |         +-------+          |
// |         |Control|          |
// |         +-------+          |
// |                            | <-- Control.bottom * kExitHeightScaleFactor
// |          Screen            |       Buffer for mouse moves or pointer events
// |                            |       before closing the fullscreen exit
// |                            |       control.
// +----------------------------+
constexpr float kExitHeightScaleFactor = 1.5f;

// +----------------------------+
// |                            |
// |                            |
// |                            | <-- kShowFullscreenExitControlHeight
// |          Screen            |       If a mouse move or pointer event is
// |                            |       above this line, show the fullscreen
// |                            |       exit control.
// |                            |
// +----------------------------+
constexpr float kShowFullscreenExitControlHeight = 3.f;

}  // namespace

FullscreenControlHost::FullscreenControlHost(BrowserView* browser_view,
                                             views::View* host_view)
    : browser_view_(browser_view),
      fullscreen_control_popup_(browser_view->GetBubbleParentView(),
                                base::Bind(&BrowserView::ExitFullscreen,
                                           base::Unretained(browser_view))) {}

FullscreenControlHost::~FullscreenControlHost() = default;

void FullscreenControlHost::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_MOVED)
    HandleFullScreenControlVisibility(event, InputEntryMethod::MOUSE);
}

void FullscreenControlHost::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_MOVED)
    HandleFullScreenControlVisibility(event, InputEntryMethod::TOUCH);
}

void FullscreenControlHost::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_LONG_TAP)
    HandleFullScreenControlVisibility(event, InputEntryMethod::TOUCH);
}

void FullscreenControlHost::Hide(bool animate) {
  fullscreen_control_popup_.Hide(animate);
}

bool FullscreenControlHost::IsVisible() const {
  return fullscreen_control_popup_.IsVisible();
}

void FullscreenControlHost::HandleFullScreenControlVisibility(
    const ui::LocatedEvent* event,
    InputEntryMethod input_entry_method) {
  if (fullscreen_control_popup_.IsAnimating() ||
      (input_entry_method_ != InputEntryMethod::NOT_ACTIVE &&
       input_entry_method_ != input_entry_method)) {
    return;
  }

  if (browser_view_->IsFullscreen()) {
    if (IsVisible()) {
      float control_bottom = static_cast<float>(
          fullscreen_control_popup_.GetFinalBounds().bottom());
      float y_limit = control_bottom * kExitHeightScaleFactor;
      if (event->y() >= y_limit)
        Hide(true);
    } else {
      if (event->y() <= kShowFullscreenExitControlHeight ||
          event->type() == ui::ET_GESTURE_LONG_TAP) {
        ShowForInputEntryMethod(input_entry_method);
      }
    }
  } else if (IsVisible()) {
    Hide(true);
  }
}

void FullscreenControlHost::ShowForInputEntryMethod(
    InputEntryMethod input_entry_method) {
  input_entry_method_ = input_entry_method;
  auto* bubble = browser_view_->exclusive_access_bubble();
  if (bubble)
    bubble->HideImmediately();
  fullscreen_control_popup_.Show(browser_view_->GetClientAreaBoundsInScreen());
}
