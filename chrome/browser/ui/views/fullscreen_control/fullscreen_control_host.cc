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

// Time to wait to hide the popup when it is triggered by touch input.
constexpr int kTouchPopupTimeoutMs = 5000;

}  // namespace

FullscreenControlHost::FullscreenControlHost(BrowserView* browser_view,
                                             views::View* host_view)
    : browser_view_(browser_view),
      fullscreen_control_popup_(
          browser_view->GetBubbleParentView(),
          base::Bind(&BrowserView::ExitFullscreen,
                     base::Unretained(browser_view)),
          base::Bind(&FullscreenControlHost::OnVisibilityChanged,
                     base::Unretained(this))) {}

FullscreenControlHost::~FullscreenControlHost() = default;

void FullscreenControlHost::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_MOVED ||
      fullscreen_control_popup_.IsAnimating() ||
      (input_entry_method_ != InputEntryMethod::NOT_ACTIVE &&
       input_entry_method_ != InputEntryMethod::MOUSE)) {
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
      if (event->y() <= kShowFullscreenExitControlHeight)
        ShowForInputEntryMethod(InputEntryMethod::MOUSE);
    }
  } else if (IsVisible()) {
    Hide(true);
  }
}

void FullscreenControlHost::OnTouchEvent(ui::TouchEvent* event) {
  if (input_entry_method_ != InputEntryMethod::TOUCH)
    return;

  DCHECK(IsVisible());

  // Hide the popup if the popup is showing and the user touches outside of the
  // popup.
  if (event->type() == ui::ET_TOUCH_PRESSED &&
      !fullscreen_control_popup_.IsAnimating()) {
    Hide(true);
  } else if (event->type() == ui::ET_TOUCH_RELEASED) {
    touch_timeout_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kTouchPopupTimeoutMs),
        base::Bind(&FullscreenControlHost::OnTouchPopupTimeout,
                   base::Unretained(this)));
  }
}

void FullscreenControlHost::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_LONG_PRESS &&
      browser_view_->IsFullscreen() && !IsVisible()) {
    ShowForInputEntryMethod(InputEntryMethod::TOUCH);
  }
}

void FullscreenControlHost::Hide(bool animate) {
  fullscreen_control_popup_.Hide(animate);
}

bool FullscreenControlHost::IsVisible() const {
  return fullscreen_control_popup_.IsVisible();
}

void FullscreenControlHost::ShowForInputEntryMethod(
    InputEntryMethod input_entry_method) {
  input_entry_method_ = input_entry_method;
  auto* bubble = browser_view_->exclusive_access_bubble();
  if (bubble)
    bubble->HideImmediately();
  fullscreen_control_popup_.Show(browser_view_->GetClientAreaBoundsInScreen());
}

void FullscreenControlHost::OnVisibilityChanged() {
  if (!IsVisible())
    input_entry_method_ = InputEntryMethod::NOT_ACTIVE;
}

void FullscreenControlHost::OnTouchPopupTimeout() {
  if (IsVisible() && !fullscreen_control_popup_.IsAnimating() &&
      input_entry_method_ == InputEntryMethod::TOUCH) {
    Hide(true);
  }
}
