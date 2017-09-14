// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_view.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

namespace {

// +----------------------------+
// |       |  Control  |        |
// |       |           |        |
// |       +-----------+        | <-- Height
// |                            | <-- Height * kExitHeightScaleFactor
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
    : DropdownBarHost(browser_view),
      browser_view_(browser_view),
      fullscreen_control_view_(new FullscreenControlView(browser_view)) {
  Init(host_view, fullscreen_control_view_, fullscreen_control_view_);
}

FullscreenControlHost::~FullscreenControlHost() = default;

bool FullscreenControlHost::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  return false;
}

bool FullscreenControlHost::CanHandleAccelerators() const {
  return false;
}

gfx::Rect FullscreenControlHost::GetDialogPosition(
    gfx::Rect avoid_overlapping_rect) {
  gfx::Rect bounds;
  GetWidgetBounds(&bounds);
  if (bounds.IsEmpty())
    return gfx::Rect();

  // Place the view horizontally centered at the top of the widget.
  int original_y = bounds.y();
  bounds.ClampToCenteredSize(view()->GetPreferredSize());
  bounds.set_y(original_y);
  return bounds;
}

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

void FullscreenControlHost::OnVisibilityChanged() {
  if (!IsVisible())
    input_entry_method_ = InputEntryMethod::NOT_ACTIVE;
}

void FullscreenControlHost::HandleFullScreenControlVisibility(
    const ui::LocatedEvent* event,
    InputEntryMethod input_entry_method) {
  if (IsAnimating() || (input_entry_method_ != InputEntryMethod::NOT_ACTIVE &&
                        input_entry_method_ != input_entry_method)) {
    return;
  }

  if (browser_view_->IsFullscreen()) {
    if (IsVisible()) {
      float control_height = static_cast<float>(view()->height());
      float y_limit = control_height * kExitHeightScaleFactor;
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
  Show(true);
}
