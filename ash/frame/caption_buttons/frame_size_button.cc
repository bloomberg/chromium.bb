// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/caption_buttons/frame_size_button.h"

#include "ash/shell_port.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "base/i18n/rtl.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The default delay between the user pressing the size button and the buttons
// adjacent to the size button morphing into buttons for snapping left and
// right.
const int kSetButtonsToSnapModeDelayMs = 150;

// The amount that a user can overshoot one of the caption buttons while in
// "snap mode" and keep the button hovered/pressed.
const int kMaxOvershootX = 200;
const int kMaxOvershootY = 50;

// Returns true if a mouse drag while in "snap mode" at |location_in_screen|
// would hover/press |button| or keep it hovered/pressed.
bool HitTestButton(const FrameCaptionButton* button,
                   const gfx::Point& location_in_screen) {
  gfx::Rect expanded_bounds_in_screen = button->GetBoundsInScreen();
  if (button->state() == views::Button::STATE_HOVERED ||
      button->state() == views::Button::STATE_PRESSED) {
    expanded_bounds_in_screen.Inset(-kMaxOvershootX, -kMaxOvershootY);
  }
  return expanded_bounds_in_screen.Contains(location_in_screen);
}

}  // namespace

FrameSizeButton::FrameSizeButton(views::ButtonListener* listener,
                                 views::Widget* frame,
                                 FrameSizeButtonDelegate* delegate)
    : FrameCaptionButton(listener, CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE),
      frame_(frame),
      delegate_(delegate),
      set_buttons_to_snap_mode_delay_ms_(kSetButtonsToSnapModeDelayMs),
      in_snap_mode_(false),
      snap_type_(SNAP_NONE) {}

FrameSizeButton::~FrameSizeButton() {}

bool FrameSizeButton::OnMousePressed(const ui::MouseEvent& event) {
  // The minimize and close buttons are set to snap left and right when snapping
  // is enabled. Do not enable snapping if the minimize button is not visible.
  // The close button is always visible.
  if (IsTriggerableEvent(event) && !in_snap_mode_ &&
      delegate_->IsMinimizeButtonVisible()) {
    StartSetButtonsToSnapModeTimer(event);
  }
  FrameCaptionButton::OnMousePressed(event);
  return true;
}

bool FrameSizeButton::OnMouseDragged(const ui::MouseEvent& event) {
  UpdateSnapType(event);
  // By default a FrameCaptionButton reverts to STATE_NORMAL once the mouse
  // leaves its bounds. Skip FrameCaptionButton's handling when
  // |in_snap_mode_| == true because we want different behavior.
  if (!in_snap_mode_)
    FrameCaptionButton::OnMouseDragged(event);
  return true;
}

void FrameSizeButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (!IsTriggerableEvent(event) || !CommitSnap(event))
    FrameCaptionButton::OnMouseReleased(event);
}

void FrameSizeButton::OnMouseCaptureLost() {
  SetButtonsToNormalMode(FrameSizeButtonDelegate::ANIMATE_YES);
  FrameCaptionButton::OnMouseCaptureLost();
}

void FrameSizeButton::OnMouseMoved(const ui::MouseEvent& event) {
  // Ignore any synthetic mouse moves during a drag.
  if (!in_snap_mode_)
    FrameCaptionButton::OnMouseMoved(event);
}

void FrameSizeButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->details().touch_points() > 1) {
    SetButtonsToNormalMode(FrameSizeButtonDelegate::ANIMATE_YES);
    return;
  }

  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    StartSetButtonsToSnapModeTimer(*event);
    // Go through FrameCaptionButton's handling so that the button gets pressed.
    FrameCaptionButton::OnGestureEvent(event);
    return;
  }

  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    UpdateSnapType(*event);
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_SCROLL_END ||
      event->type() == ui::ET_SCROLL_FLING_START ||
      event->type() == ui::ET_GESTURE_END) {
    if (CommitSnap(*event)) {
      event->SetHandled();
      return;
    }
  }

  FrameCaptionButton::OnGestureEvent(event);
}

void FrameSizeButton::StartSetButtonsToSnapModeTimer(
    const ui::LocatedEvent& event) {
  set_buttons_to_snap_mode_timer_event_location_ = event.location();
  if (set_buttons_to_snap_mode_delay_ms_ == 0) {
    AnimateButtonsToSnapMode();
  } else {
    set_buttons_to_snap_mode_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(set_buttons_to_snap_mode_delay_ms_),
        this, &FrameSizeButton::AnimateButtonsToSnapMode);
  }
}

void FrameSizeButton::AnimateButtonsToSnapMode() {
  SetButtonsToSnapMode(FrameSizeButtonDelegate::ANIMATE_YES);
}

void FrameSizeButton::SetButtonsToSnapMode(
    FrameSizeButtonDelegate::Animate animate) {
  in_snap_mode_ = true;

  // When using a right-to-left layout the close button is left of the size
  // button and the minimize button is right of the size button.
  if (base::i18n::IsRTL()) {
    delegate_->SetButtonIcons(CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
                              CAPTION_BUTTON_ICON_LEFT_SNAPPED, animate);
  } else {
    delegate_->SetButtonIcons(CAPTION_BUTTON_ICON_LEFT_SNAPPED,
                              CAPTION_BUTTON_ICON_RIGHT_SNAPPED, animate);
  }
}

void FrameSizeButton::UpdateSnapType(const ui::LocatedEvent& event) {
  if (!in_snap_mode_) {
    // Set the buttons adjacent to the size button to snap left and right early
    // if the user drags past the drag threshold.
    // |set_buttons_to_snap_mode_timer_| is checked to avoid entering the snap
    // mode as a result of an unsupported drag type (e.g. only the right mouse
    // button is pressed).
    gfx::Vector2d delta(event.location() -
                        set_buttons_to_snap_mode_timer_event_location_);
    if (!set_buttons_to_snap_mode_timer_.IsRunning() ||
        !views::View::ExceededDragThreshold(delta)) {
      return;
    }
    AnimateButtonsToSnapMode();
  }

  gfx::Point event_location_in_screen(event.location());
  views::View::ConvertPointToScreen(this, &event_location_in_screen);
  const FrameCaptionButton* to_hover =
      GetButtonToHover(event_location_in_screen);
  bool press_size_button =
      to_hover || HitTestButton(this, event_location_in_screen);

  if (to_hover) {
    // Progress the minimize and close icon morph animations to the end if they
    // are in progress.
    SetButtonsToSnapMode(FrameSizeButtonDelegate::ANIMATE_NO);
  }

  delegate_->SetHoveredAndPressedButtons(to_hover,
                                         press_size_button ? this : NULL);

  snap_type_ = SNAP_NONE;
  if (to_hover) {
    switch (to_hover->icon()) {
      case CAPTION_BUTTON_ICON_LEFT_SNAPPED:
        snap_type_ = SNAP_LEFT;
        break;
      case CAPTION_BUTTON_ICON_RIGHT_SNAPPED:
        snap_type_ = SNAP_RIGHT;
        break;
      case CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE:
      case CAPTION_BUTTON_ICON_MINIMIZE:
      case CAPTION_BUTTON_ICON_CLOSE:
      case CAPTION_BUTTON_ICON_BACK:
      case CAPTION_BUTTON_ICON_LOCATION:
      case CAPTION_BUTTON_ICON_COUNT:
        NOTREACHED();
        break;
    }
  }

  if (snap_type_ == SNAP_LEFT || snap_type_ == SNAP_RIGHT) {
    aura::Window* window = frame_->GetNativeWindow();
    if (!phantom_window_controller_.get()) {
      phantom_window_controller_ =
          base::MakeUnique<PhantomWindowController>(window);
    }
    gfx::Rect phantom_bounds_in_screen =
        (snap_type_ == SNAP_LEFT)
            ? wm::GetDefaultLeftSnappedWindowBoundsInParent(window)
            : wm::GetDefaultRightSnappedWindowBoundsInParent(window);
    ::wm::ConvertRectToScreen(window->parent(), &phantom_bounds_in_screen);
    phantom_window_controller_->Show(phantom_bounds_in_screen);
  } else {
    phantom_window_controller_.reset();
  }
}

const FrameCaptionButton* FrameSizeButton::GetButtonToHover(
    const gfx::Point& event_location_in_screen) const {
  const FrameCaptionButton* closest_button =
      delegate_->GetButtonClosestTo(event_location_in_screen);
  if ((closest_button->icon() == CAPTION_BUTTON_ICON_LEFT_SNAPPED ||
       closest_button->icon() == CAPTION_BUTTON_ICON_RIGHT_SNAPPED) &&
      HitTestButton(closest_button, event_location_in_screen)) {
    return closest_button;
  }
  return NULL;
}

bool FrameSizeButton::CommitSnap(const ui::LocatedEvent& event) {
  // The position of |event| may be different than the position of the previous
  // event.
  UpdateSnapType(event);

  if (in_snap_mode_ && (snap_type_ == SNAP_LEFT || snap_type_ == SNAP_RIGHT)) {
    wm::WindowState* window_state =
        wm::GetWindowState(frame_->GetNativeWindow());
    const wm::WMEvent snap_event(snap_type_ == SNAP_LEFT
                                     ? wm::WM_EVENT_SNAP_LEFT
                                     : wm::WM_EVENT_SNAP_RIGHT);
    window_state->OnWMEvent(&snap_event);
    ShellPort::Get()->RecordUserMetricsAction(
        snap_type_ == SNAP_LEFT ? UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_LEFT
                                : UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_RIGHT);
    SetButtonsToNormalMode(FrameSizeButtonDelegate::ANIMATE_NO);
    return true;
  }
  SetButtonsToNormalMode(FrameSizeButtonDelegate::ANIMATE_YES);
  return false;
}

void FrameSizeButton::SetButtonsToNormalMode(
    FrameSizeButtonDelegate::Animate animate) {
  in_snap_mode_ = false;
  snap_type_ = SNAP_NONE;
  set_buttons_to_snap_mode_timer_.Stop();
  delegate_->SetButtonsToNormal(animate);
  phantom_window_controller_.reset();
}

}  // namespace ash
