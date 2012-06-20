// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/frame_maximize_button.h"

#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "grit/ui_resources_standard.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

using ash::internal::SnapSizer;

namespace ash {

namespace {

// Delay before forcing an update of the snap location.
const int kUpdateDelayMS = 400;

}

// EscapeEventFilter is installed on the RootWindow to track when the escape key
// is pressed. We use an EventFilter for this as the FrameMaximizeButton
// normally does not get focus.
class FrameMaximizeButton::EscapeEventFilter : public aura::EventFilter {
 public:
  explicit EscapeEventFilter(FrameMaximizeButton* button);
  virtual ~EscapeEventFilter();

  // EventFilter overrides:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(
      aura::Window* target,
      aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

 private:
  FrameMaximizeButton* button_;

  DISALLOW_COPY_AND_ASSIGN(EscapeEventFilter);
};

FrameMaximizeButton::EscapeEventFilter::EscapeEventFilter(
    FrameMaximizeButton* button)
    : button_(button) {
  Shell::GetInstance()->AddEnvEventFilter(this);
}

FrameMaximizeButton::EscapeEventFilter::~EscapeEventFilter() {
  Shell::GetInstance()->RemoveEnvEventFilter(this);
}

bool FrameMaximizeButton::EscapeEventFilter::PreHandleKeyEvent(
    aura::Window* target,
    aura::KeyEvent* event) {
  if (event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_ESCAPE) {
    button_->Cancel();
  }
  return false;
}

bool FrameMaximizeButton::EscapeEventFilter::PreHandleMouseEvent(
    aura::Window* target,
    aura::MouseEvent* event) {
  return false;
}

ui::TouchStatus FrameMaximizeButton::EscapeEventFilter::PreHandleTouchEvent(
      aura::Window* target,
      aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus FrameMaximizeButton::EscapeEventFilter::PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;
}

// FrameMaximizeButton ---------------------------------------------------------

FrameMaximizeButton::FrameMaximizeButton(views::ButtonListener* listener,
                                         views::NonClientFrameView* frame)
    : ImageButton(listener),
      frame_(frame),
      is_snap_enabled_(false),
      exceeded_drag_threshold_(false),
      snap_type_(SNAP_NONE) {
  // TODO(sky): nuke this. It's temporary while we don't have good images.
  SetImageAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_FRAME_MAXIMIZE_BUTTON_TOOLTIP));
}

FrameMaximizeButton::~FrameMaximizeButton() {
}

bool FrameMaximizeButton::OnMousePressed(const views::MouseEvent& event) {
  is_snap_enabled_ = event.IsLeftMouseButton();
  if (is_snap_enabled_)
    ProcessStartEvent(event);
  ImageButton::OnMousePressed(event);
  return true;
}

void FrameMaximizeButton::OnMouseEntered(const views::MouseEvent& event) {
  ImageButton::OnMouseEntered(event);
}

void FrameMaximizeButton::OnMouseExited(const views::MouseEvent& event) {
  ImageButton::OnMouseExited(event);
}

bool FrameMaximizeButton::OnMouseDragged(const views::MouseEvent& event) {
  if (is_snap_enabled_)
    ProcessUpdateEvent(event);
  return ImageButton::OnMouseDragged(event);
}

void FrameMaximizeButton::OnMouseReleased(const views::MouseEvent& event) {
  if (!ProcessEndEvent(event))
    ImageButton::OnMouseReleased(event);
}

void FrameMaximizeButton::OnMouseCaptureLost() {
  Cancel();
  ImageButton::OnMouseCaptureLost();
}

ui::GestureStatus FrameMaximizeButton::OnGestureEvent(
    const views::GestureEvent& event) {
  if (event.type() == ui::ET_GESTURE_TAP_DOWN) {
    is_snap_enabled_ = true;
    ProcessStartEvent(event);
    return ui::GESTURE_STATUS_CONSUMED;
  }

  if (event.type() == ui::ET_GESTURE_TAP ||
      event.type() == ui::ET_GESTURE_SCROLL_END) {
    if (event.type() == ui::ET_GESTURE_TAP)
      snap_type_ = SnapTypeForLocation(event.location());
    ProcessEndEvent(event);
    return ui::GESTURE_STATUS_CONSUMED;
  }

  if (event.type() == ui::ET_GESTURE_END &&
      event.details().touch_points() == 1 &&
      is_snap_enabled_) {
    snap_type_ = SnapTypeForLocation(event.location());
    ProcessEndEvent(event);
    return ui::GESTURE_STATUS_CONSUMED;
  }

  if (event.type() == ui::ET_GESTURE_SCROLL_UPDATE ||
      event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    ProcessUpdateEvent(event);
    return ui::GESTURE_STATUS_CONSUMED;
  }

  return ImageButton::OnGestureEvent(event);
}

gfx::ImageSkia FrameMaximizeButton::GetImageToPaint(float scale) {
  if (is_snap_enabled_) {
    int id = 0;
    if (frame_->GetWidget()->IsMaximized()) {
      switch (snap_type_) {
        case SNAP_LEFT:
          id = IDR_AURA_WINDOW_MAXIMIZED_SNAP_LEFT_P;
          break;
        case SNAP_RIGHT:
          id = IDR_AURA_WINDOW_MAXIMIZED_SNAP_RIGHT_P;
          break;
        case SNAP_MAXIMIZE:
        case SNAP_RESTORE:
        case SNAP_NONE:
          id = IDR_AURA_WINDOW_MAXIMIZED_SNAP_P;
          break;
        case SNAP_MINIMIZE:
          id = IDR_AURA_WINDOW_MAXIMIZED_SNAP_MINIMIZE_P;
          break;
        default:
          NOTREACHED();
      }
    } else {
      switch (snap_type_) {
        case SNAP_LEFT:
          id = IDR_AURA_WINDOW_SNAP_LEFT_P;
          break;
        case SNAP_RIGHT:
          id = IDR_AURA_WINDOW_SNAP_RIGHT_P;
          break;
        case SNAP_MAXIMIZE:
        case SNAP_RESTORE:
        case SNAP_NONE:
          id = IDR_AURA_WINDOW_SNAP_P;
          break;
        case SNAP_MINIMIZE:
          id = IDR_AURA_WINDOW_SNAP_MINIMIZE_P;
          break;
        default:
          NOTREACHED();
      }
    }
    return *ResourceBundle::GetSharedInstance().GetImageNamed(id).ToImageSkia();
  }
  // Hot and pressed states handled by regular ImageButton.
  return ImageButton::GetImageToPaint(scale);
}

void FrameMaximizeButton::ProcessStartEvent(const views::LocatedEvent& event) {
  DCHECK(is_snap_enabled_);
  snap_sizer_.reset(NULL);
  InstallEventFilter();
  snap_type_ = SNAP_NONE;
  press_location_ = event.location();
  exceeded_drag_threshold_ = false;
  update_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kUpdateDelayMS),
      this,
      &FrameMaximizeButton::UpdateSnapFromEventLocation);
}

void FrameMaximizeButton::ProcessUpdateEvent(const views::LocatedEvent& event) {
  DCHECK(is_snap_enabled_);
  int delta_x = event.x() - press_location_.x();
  int delta_y = event.y() - press_location_.y();
  if (!exceeded_drag_threshold_) {
    exceeded_drag_threshold_ =
        views::View::ExceededDragThreshold(delta_x, delta_y);
  }
  if (exceeded_drag_threshold_)
    UpdateSnap(event.location());
}

bool FrameMaximizeButton::ProcessEndEvent(const views::LocatedEvent& event) {
  update_timer_.Stop();
  UninstallEventFilter();
  bool should_snap = is_snap_enabled_;
  is_snap_enabled_ = false;

  if (!should_snap || snap_type_ == SNAP_NONE)
    return false;

  SetState(BS_NORMAL);
  // SetState will not call SchedulePaint() if state was already set to
  // BS_NORMAL during a drag.
  SchedulePaint();
  phantom_window_.reset();
  Snap();
  return true;
}

void FrameMaximizeButton::Cancel() {
  UninstallEventFilter();
  is_snap_enabled_ = false;
  phantom_window_.reset();
  snap_sizer_.reset();
  update_timer_.Stop();
  SchedulePaint();
}

void FrameMaximizeButton::InstallEventFilter() {
  if (escape_event_filter_.get())
    return;

  escape_event_filter_.reset(new EscapeEventFilter(this));
}

void FrameMaximizeButton::UninstallEventFilter() {
  escape_event_filter_.reset(NULL);
}

void FrameMaximizeButton::UpdateSnapFromEventLocation() {
  // If the drag threshold has been exceeded the snap location is up to date.
  if (exceeded_drag_threshold_)
    return;
  exceeded_drag_threshold_ = true;
  UpdateSnap(press_location_);
}

void FrameMaximizeButton::UpdateSnap(const gfx::Point& location) {
  SnapType type = SnapTypeForLocation(location);
  if (type == snap_type_) {
    if (snap_sizer_.get()) {
      snap_sizer_->Update(LocationForSnapSizer(location));
      phantom_window_->Show(snap_sizer_->target_bounds());
    }
    return;
  }

  snap_type_ = type;
  snap_sizer_.reset();
  SchedulePaint();

  if (snap_type_ == SNAP_NONE) {
    phantom_window_.reset();
    return;
  }

  if (snap_type_ == SNAP_LEFT || snap_type_ == SNAP_RIGHT) {
    SnapSizer::Edge snap_edge = snap_type_ == SNAP_LEFT ?
        SnapSizer::LEFT_EDGE : SnapSizer::RIGHT_EDGE;
    int grid_size = Shell::GetInstance()->GetGridSize();
    snap_sizer_.reset(new SnapSizer(frame_->GetWidget()->GetNativeWindow(),
                                    LocationForSnapSizer(location),
                                    snap_edge, grid_size));
  }
  if (!phantom_window_.get()) {
    phantom_window_.reset(new internal::PhantomWindowController(
                              frame_->GetWidget()->GetNativeWindow()));
  }
  phantom_window_->Show(BoundsForType(snap_type_));
}

FrameMaximizeButton::SnapType FrameMaximizeButton::SnapTypeForLocation(
    const gfx::Point& location) const {
  int delta_x = location.x() - press_location_.x();
  int delta_y = location.y() - press_location_.y();
  if (!views::View::ExceededDragThreshold(delta_x, delta_y))
    return !frame_->GetWidget()->IsMaximized() ? SNAP_MAXIMIZE : SNAP_RESTORE;
  else if (delta_x < 0 && delta_y > delta_x && delta_y < -delta_x)
    return SNAP_LEFT;
  else if (delta_x > 0 && delta_y > -delta_x && delta_y < delta_x)
    return SNAP_RIGHT;
  else if (delta_y > 0)
    return SNAP_MINIMIZE;
  return !frame_->GetWidget()->IsMaximized() ? SNAP_MAXIMIZE : SNAP_RESTORE;
}

gfx::Rect FrameMaximizeButton::BoundsForType(SnapType type) const {
  aura::Window* window = frame_->GetWidget()->GetNativeWindow();
  switch (type) {
    case SNAP_LEFT:
    case SNAP_RIGHT:
      return snap_sizer_->target_bounds();
    case SNAP_MAXIMIZE:
      return ScreenAsh::GetMaximizedWindowBounds(window);
    case SNAP_MINIMIZE: {
      Launcher* launcher = Shell::GetInstance()->launcher();
      gfx::Rect item_rect(launcher->GetScreenBoundsOfItemIconForWindow(window));
      if (!item_rect.IsEmpty()) {
        // PhantomWindowController insets slightly, outset it so the phantom
        // doesn't appear inset.
        item_rect.Inset(-8, -8);
        return item_rect;
      }
      return launcher->widget()->GetWindowScreenBounds();
    }
    case SNAP_RESTORE: {
      const gfx::Rect* restore = GetRestoreBounds(window);
      return restore ? *restore : frame_->GetWidget()->GetWindowScreenBounds();
    }
    case SNAP_NONE:
      NOTREACHED();
  }
  return gfx::Rect();
}

gfx::Point FrameMaximizeButton::LocationForSnapSizer(
    const gfx::Point& location) const {
  gfx::Point result(location);
  views::View::ConvertPointToScreen(this, &result);
  return result;
}

void FrameMaximizeButton::Snap() {
  switch (snap_type_) {
    case SNAP_LEFT:
    case SNAP_RIGHT:
      if (frame_->GetWidget()->IsMaximized()) {
        ash::SetRestoreBounds(frame_->GetWidget()->GetNativeWindow(),
                              BoundsForType(snap_type_));
        frame_->GetWidget()->Restore();
      } else {
        frame_->GetWidget()->SetBounds(BoundsForType(snap_type_));
      }
      break;
    case SNAP_MAXIMIZE:
      frame_->GetWidget()->Maximize();
      break;
    case SNAP_MINIMIZE:
      frame_->GetWidget()->Minimize();
      break;
    case SNAP_RESTORE:
      frame_->GetWidget()->Restore();
      break;
    case SNAP_NONE:
      NOTREACHED();
  }
}

}  // namespace ash
