// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/frame_maximize_button.h"

#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ash/launcher/launcher.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "grit/ui_resources.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

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
  Shell::GetInstance()->AddRootWindowEventFilter(this);
}

FrameMaximizeButton::EscapeEventFilter::~EscapeEventFilter() {
  Shell::GetInstance()->RemoveRootWindowEventFilter(this);
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

FrameMaximizeButton::FrameMaximizeButton(views::ButtonListener* listener)
    : ImageButton(listener),
      is_snap_enabled_(false),
      exceeded_drag_threshold_(false),
      snap_type_(SNAP_NONE) {
  // TODO(sky): nuke this. It's temporary while we don't have good images.
  SetImageAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
}

FrameMaximizeButton::~FrameMaximizeButton() {
}

bool FrameMaximizeButton::OnMousePressed(const views::MouseEvent& event) {
  is_snap_enabled_ = event.IsLeftMouseButton();
  if (is_snap_enabled_) {
    InstallEventFilter();
    snap_type_ = SNAP_NONE;
    press_location_ = event.location();
    exceeded_drag_threshold_ = false;
  }
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
  if (is_snap_enabled_) {
    int delta_x = event.location().x() - press_location_.x();
    int delta_y = event.location().y() - press_location_.y();
    if (!exceeded_drag_threshold_) {
      exceeded_drag_threshold_ =
          views::View::ExceededDragThreshold(delta_x, delta_y);
    }
    if (exceeded_drag_threshold_)
      UpdateSnap(delta_x, delta_y);
  }
  return ImageButton::OnMouseDragged(event);
}

void FrameMaximizeButton::OnMouseReleased(const views::MouseEvent& event) {
  UninstallEventFilter();
  bool should_snap = is_snap_enabled_;
  is_snap_enabled_ = false;
  if (should_snap && snap_type_ != SNAP_NONE) {
    SetState(BS_NORMAL);
    phantom_window_.reset();
    Snap();
  } else {
    ImageButton::OnMouseReleased(event);
  }
}

void FrameMaximizeButton::OnMouseCaptureLost() {
  Cancel();
  ImageButton::OnMouseCaptureLost();
}

SkBitmap FrameMaximizeButton::GetImageToPaint() {
  if (is_snap_enabled_) {
    int id = 0;
    if (GetWidget()->IsMaximized()) {
      switch (snap_type_) {
        case SNAP_LEFT:
          id = IDR_AURA_WINDOW_MAXIMIZED_RESTORE_SNAP_LEFT_P;
          break;
        case SNAP_RIGHT:
          id = IDR_AURA_WINDOW_MAXIMIZED_RESTORE_SNAP_RIGHT_P;
          break;
        case SNAP_MAXIMIZE:
        case SNAP_NONE:
          id = IDR_AURA_WINDOW_MAXIMIZED_RESTORE_SNAP_P;
          break;
        case SNAP_MINIMIZE:
          id = IDR_AURA_WINDOW_MAXIMIZED_RESTORE_SNAP_MINIMIZE_P;
          break;
        default:
          NOTREACHED();
      }
    } else {
      switch (snap_type_) {
        case SNAP_LEFT:
          id = IDR_AURA_WINDOW_MAXIMIZED_SNAP_LEFT_P;
          break;
        case SNAP_RIGHT:
          id = IDR_AURA_WINDOW_MAXIMIZED_SNAP_RIGHT_P;
          break;
        case SNAP_MAXIMIZE:
        case SNAP_NONE:
          id = IDR_AURA_WINDOW_MAXIMIZED_SNAP_P;
          break;
        case SNAP_MINIMIZE:
          id = IDR_AURA_WINDOW_MAXIMIZED_SNAP_MINIMIZE_P;
          break;
        default:
          NOTREACHED();
      }
    }
    return *ResourceBundle::GetSharedInstance().GetImageNamed(id).ToSkBitmap();
  }
  return ImageButton::GetImageToPaint();
}

void FrameMaximizeButton::Cancel() {
  UninstallEventFilter();
  is_snap_enabled_ = false;
  phantom_window_.reset();
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

void FrameMaximizeButton::UpdateSnap(int delta_x, int delta_y) {
  SnapType type = SnapTypeForDelta(delta_x, delta_y);
  if (type == snap_type_)
    return;

  SchedulePaint();

  if (type == SNAP_NONE) {
    phantom_window_.reset();
    snap_type_ = SNAP_NONE;
    return;
  }

  snap_type_ = type;
  if (!phantom_window_.get()) {
    phantom_window_.reset(new internal::PhantomWindowController(
                              GetWidget()->GetNativeWindow(),
                              internal::PhantomWindowController::TYPE_EDGE, 0));
  }
  phantom_window_->Show(BoundsForType(snap_type_));
}

FrameMaximizeButton::SnapType FrameMaximizeButton::SnapTypeForDelta(
    int delta_x,
    int delta_y) const {
  if (!views::View::ExceededDragThreshold(delta_x, delta_y))
    return GetWidget()->IsMaximized() ? SNAP_NONE : SNAP_MAXIMIZE;
  else if (delta_x < 0 && delta_y > delta_x && delta_y < -delta_x)
    return SNAP_LEFT;
  else if (delta_x > 0 && delta_y > -delta_x && delta_y < delta_x)
    return SNAP_RIGHT;
  else if (delta_y > 0)
    return SNAP_MINIMIZE;
  return GetWidget()->IsMaximized() ? SNAP_NONE : SNAP_MAXIMIZE;
}

gfx::Rect FrameMaximizeButton::BoundsForType(SnapType type) const {
  aura::Window* window = GetWidget()->GetNativeWindow();
  int grid_size = Shell::GetInstance()->GetGridSize();
  switch (type) {
    case SNAP_LEFT:
      return internal::WorkspaceWindowResizer::GetBoundsForWindowAlongEdge(
          window, internal::WorkspaceWindowResizer::LEFT_EDGE, grid_size);
    case SNAP_RIGHT:
      return internal::WorkspaceWindowResizer::GetBoundsForWindowAlongEdge(
          window, internal::WorkspaceWindowResizer::RIGHT_EDGE, grid_size);
    case SNAP_MAXIMIZE:
      return gfx::Screen::GetMonitorWorkAreaNearestWindow(window);
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
    default:
      NOTREACHED();
  }
  return gfx::Rect();
}

void FrameMaximizeButton::Snap() {
  switch (snap_type_) {
    case SNAP_LEFT:
    case SNAP_RIGHT:
      if (GetWidget()->IsMaximized()) {
        ash::SetRestoreBounds(GetWidget()->GetNativeWindow(),
                              BoundsForType(snap_type_));
        GetWidget()->Restore();
      } else {
        GetWidget()->SetBounds(BoundsForType(snap_type_));
      }
      break;
    case SNAP_MAXIMIZE:
      GetWidget()->Maximize();
      break;
    case SNAP_MINIMIZE:
      GetWidget()->Minimize();
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace ash
