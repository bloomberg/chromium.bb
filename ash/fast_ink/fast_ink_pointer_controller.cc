// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_pointer_controller.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/palette/palette_utils.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The default amount of time used to estimate time from VSYNC event to when
// visible light can be noticed by the user. This is used when a device
// specific estimate was not provided using --estimated-presentation-delay.
const int kDefaultPresentationDelayMs = 18;

base::TimeDelta GetPresentationDelay() {
  int64_t presentation_delay_ms;
  // Use device specific presentation delay if specified.
  std::string presentation_delay_string =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAshEstimatedPresentationDelay);
  if (!base::StringToInt64(presentation_delay_string, &presentation_delay_ms))
    presentation_delay_ms = kDefaultPresentationDelayMs;
  return base::TimeDelta::FromMilliseconds(presentation_delay_ms);
}

}  // namespace

FastInkPointerController::FastInkPointerController()
    : presentation_delay_(GetPresentationDelay()) {
  Shell::Get()->AddPreTargetHandler(this);
}

FastInkPointerController::~FastInkPointerController() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void FastInkPointerController::SetEnabled(bool enabled) {
  enabled_ = enabled;
  // Not calling DestroyPointerView when disabling, leaving the control over
  // the pointer view lifetime to the specific controller implementation.
  // For instance, a controller might prefer to keep the pointer view around
  // while it is being animated away.
}

bool FastInkPointerController::CanStartNewGesture(ui::TouchEvent* event) {
  // 1) The stylus is pressed
  // 2) The stylus is moving, but the pointer session has not started yet
  // (most likely because the preceding press event was consumed by another
  // handler).
  return (event->type() == ui::ET_TOUCH_PRESSED ||
          (event->type() == ui::ET_TOUCH_MOVED && !GetPointerView()));
}

void FastInkPointerController::OnTouchEvent(ui::TouchEvent* event) {
  if (!enabled_)
    return;

  if (event->pointer_details().pointer_type !=
      ui::EventPointerType::POINTER_TYPE_PEN)
    return;

  if (event->type() != ui::ET_TOUCH_MOVED &&
      event->type() != ui::ET_TOUCH_PRESSED &&
      event->type() != ui::ET_TOUCH_RELEASED)
    return;

  // Find the root window that the event was captured on. We never need to
  // switch between different root windows because it is not physically possible
  // to seamlessly drag a stylus between two displays like it is with a mouse.
  aura::Window* root_window =
      static_cast<aura::Window*>(event->target())->GetRootWindow();

  if (CanStartNewGesture(event)) {
    // Ignore events over the palette.
    if (palette_utils::PaletteContainsPointInScreen(event->root_location()))
      return;
    DestroyPointerView();
    CreatePointerView(presentation_delay_, root_window);
  } else {
    views::View* pointer_view = GetPointerView();
    if (!pointer_view)
      return;
    views::Widget* widget = pointer_view->GetWidget();
    if (widget->IsClosed() ||
        widget->GetNativeWindow()->GetRootWindow() != root_window) {
      // The pointer widget is longer valid, end the current pointer session.
      DestroyPointerView();
      return;
    }
  }

  UpdatePointerView(event);
  event->StopPropagation();
}

}  // namespace ash
