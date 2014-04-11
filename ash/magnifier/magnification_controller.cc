// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnification_controller.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accessibility_delegate.h"
#include "ash/ash_switches.h"
#include "ash/display/root_window_transformers.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/root_window_transformer.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/command_line.h"
#include "base/synchronization/waitable_event.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/point_f.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/compound_event_filter.h"

namespace {

const float kMaxMagnifiedScale = 4.0f;
const float kMaxMagnifiedScaleThreshold = 4.0f;
const float kMinMagnifiedScaleThreshold = 1.1f;
const float kNonMagnifiedScale = 1.0f;

const float kInitialMagnifiedScale = 2.0f;
const float kScrollScaleChangeFactor = 0.05f;

// Threadshold of panning. If the cursor moves to within pixels (in DIP) of
// |kPanningMergin| from the edge, the view-port moves.
const int kPanningMergin = 100;

void MoveCursorTo(aura::WindowTreeHost* host, const gfx::Point& root_location) {
  gfx::Point3F host_location_3f(root_location);
  host->GetRootTransform().TransformPoint(&host_location_3f);
  host->MoveCursorToHostLocation(
      gfx::ToCeiledPoint(host_location_3f.AsPointF()));
}

}  // namespace

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// MagnificationControllerImpl:

class MagnificationControllerImpl : virtual public MagnificationController,
                                    public ui::EventHandler,
                                    public ui::ImplicitAnimationObserver,
                                    public aura::WindowObserver {
 public:
  MagnificationControllerImpl();
  virtual ~MagnificationControllerImpl();

  // MagnificationController overrides:
  virtual void SetEnabled(bool enabled) OVERRIDE;
  virtual bool IsEnabled() const OVERRIDE;
  virtual void SetScale(float scale, bool animate) OVERRIDE;
  virtual float GetScale() const OVERRIDE { return scale_; }
  virtual void MoveWindow(int x, int y, bool animate) OVERRIDE;
  virtual void MoveWindow(const gfx::Point& point, bool animate) OVERRIDE;
  virtual gfx::Point GetWindowPosition() const OVERRIDE {
    return gfx::ToFlooredPoint(origin_);
  }
  virtual void SetScrollDirection(ScrollDirection direction) OVERRIDE;

  // For test
  virtual gfx::Point GetPointOfInterestForTesting() OVERRIDE {
    return point_of_interest_;
  }

 private:
  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  // aura::WindowObserver overrides:
  virtual void OnWindowDestroying(aura::Window* root_window) OVERRIDE;
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  // Redraws the magnification window with the given origin position and the
  // given scale. Returns true if the window is changed; otherwise, false.
  // These methods should be called internally just after the scale and/or
  // the position are changed to redraw the window.
  bool Redraw(const gfx::PointF& position, float scale, bool animate);
  bool RedrawDIP(const gfx::PointF& position, float scale, bool animate);

  // 1) If the screen is scrolling (i.e. animating) and should scroll further,
  // it does nothing.
  // 2) If the screen is scrolling (i.e. animating) and the direction is NONE,
  // it stops the scrolling animation.
  // 3) If the direction is set to value other than NONE, it starts the
  // scrolling/ animation towards that direction.
  void StartOrStopScrollIfNecessary();

  // Redraw with the given zoom scale keeping the mouse cursor location. In
  // other words, zoom (or unzoom) centering around the cursor.
  void RedrawKeepingMousePosition(float scale, bool animate);

  void OnMouseMove(const gfx::Point& location);

  // Move the mouse cursot to the given point. Actual move will be done when
  // the animation is completed. This should be called after animation is
  // started.
  void AfterAnimationMoveCursorTo(const gfx::Point& location);

  // Switch Magnified RootWindow to |new_root_window|. This does following:
  //  - Unzoom the current root_window.
  //  - Zoom the given new root_window |new_root_window|.
  //  - Switch the target window from current window to |new_root_window|.
  void SwitchTargetRootWindow(aura::Window* new_root_window,
                              bool redraw_original_root_window);

  // Returns if the magnification scale is 1.0 or not (larger then 1.0).
  bool IsMagnified() const;

  // Returns the rect of the magnification window.
  gfx::RectF GetWindowRectDIP(float scale) const;
  // Returns the size of the root window.
  gfx::Size GetHostSizeDIP() const;

  // Correct the givin scale value if nessesary.
  void ValidateScale(float* scale);

  // ui::EventHandler overrides:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

  // Target root window. This must not be NULL.
  aura::Window* root_window_;

  // True if the magnified window is currently animating a change. Otherwise,
  // false.
  bool is_on_animation_;

  bool is_enabled_;

  // True if the cursor needs to move the given position after the animation
  // will be finished. When using this, set |position_after_animation_| as well.
  bool move_cursor_after_animation_;
  // Stores the position of cursor to be moved after animation.
  gfx::Point position_after_animation_;

  // Stores the last mouse cursor (or last touched) location. This value is
  // used on zooming to keep this location visible.
  gfx::Point point_of_interest_;

  // Current scale, origin (left-top) position of the magnification window.
  float scale_;
  gfx::PointF origin_;

  ScrollDirection scroll_direction_;

  DISALLOW_COPY_AND_ASSIGN(MagnificationControllerImpl);
};

////////////////////////////////////////////////////////////////////////////////
// MagnificationControllerImpl:

MagnificationControllerImpl::MagnificationControllerImpl()
    : root_window_(Shell::GetPrimaryRootWindow()),
      is_on_animation_(false),
      is_enabled_(false),
      move_cursor_after_animation_(false),
      scale_(kNonMagnifiedScale),
      scroll_direction_(SCROLL_NONE) {
  Shell::GetInstance()->AddPreTargetHandler(this);
  root_window_->AddObserver(this);
  point_of_interest_ = root_window_->bounds().CenterPoint();
}

MagnificationControllerImpl::~MagnificationControllerImpl() {
  root_window_->RemoveObserver(this);

  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void MagnificationControllerImpl::RedrawKeepingMousePosition(
    float scale, bool animate) {
  gfx::Point mouse_in_root = point_of_interest_;

  // mouse_in_root is invalid value when the cursor is hidden.
  if (!root_window_->bounds().Contains(mouse_in_root))
    mouse_in_root = root_window_->bounds().CenterPoint();

  const gfx::PointF origin =
      gfx::PointF(mouse_in_root.x() -
                      (scale_ / scale) * (mouse_in_root.x() - origin_.x()),
                  mouse_in_root.y() -
                      (scale_ / scale) * (mouse_in_root.y() - origin_.y()));
  bool changed = RedrawDIP(origin, scale, animate);
  if (changed)
    AfterAnimationMoveCursorTo(mouse_in_root);
}

bool MagnificationControllerImpl::Redraw(const gfx::PointF& position,
                                         float scale,
                                         bool animate) {
  const gfx::PointF position_in_dip =
      ui::ConvertPointToDIP(root_window_->layer(), position);
  return RedrawDIP(position_in_dip, scale, animate);
}

bool MagnificationControllerImpl::RedrawDIP(const gfx::PointF& position_in_dip,
                                            float scale,
                                            bool animate) {
  DCHECK(root_window_);

  float x = position_in_dip.x();
  float y = position_in_dip.y();

  ValidateScale(&scale);

  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;

  const gfx::Size host_size_in_dip = GetHostSizeDIP();
  const gfx::SizeF window_size_in_dip = GetWindowRectDIP(scale).size();
  float max_x = host_size_in_dip.width() - window_size_in_dip.width();
  float max_y = host_size_in_dip.height() - window_size_in_dip.height();
  if (x > max_x)
    x = max_x;
  if (y > max_y)
    y = max_y;

  // Does nothing if both the origin and the scale are not changed.
  if (origin_.x() == x  &&
      origin_.y() == y &&
      scale == scale_) {
    return false;
  }

  origin_.set_x(x);
  origin_.set_y(y);
  scale_ = scale;

  // Creates transform matrix.
  gfx::Transform transform;
  // Flips the signs intentionally to convert them from the position of the
  // magnification window.
  transform.Scale(scale_, scale_);
  transform.Translate(-origin_.x(), -origin_.y());

  ui::ScopedLayerAnimationSettings settings(
      root_window_->layer()->GetAnimator());
  settings.AddObserver(this);
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTweenType(gfx::Tween::EASE_OUT);
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(animate ? 100 : 0));

  gfx::Display display =
      Shell::GetScreen()->GetDisplayNearestWindow(root_window_);
  scoped_ptr<RootWindowTransformer> transformer(
      CreateRootWindowTransformerForDisplay(root_window_, display));
  GetRootWindowController(root_window_)->ash_host()->SetRootWindowTransformer(
      transformer.Pass());

  if (animate)
    is_on_animation_ = true;

  return true;
}

void MagnificationControllerImpl::StartOrStopScrollIfNecessary() {
  // This value controls the scrolling speed.
  const int kMoveOffset = 40;
  if (is_on_animation_) {
    if (scroll_direction_ == SCROLL_NONE)
      root_window_->layer()->GetAnimator()->StopAnimating();
    return;
  }

  gfx::PointF new_origin = origin_;
  switch (scroll_direction_) {
    case SCROLL_NONE:
      // No need to take action.
      return;
    case SCROLL_LEFT:
      new_origin.Offset(-kMoveOffset, 0);
      break;
    case SCROLL_RIGHT:
      new_origin.Offset(kMoveOffset, 0);
      break;
    case SCROLL_UP:
      new_origin.Offset(0, -kMoveOffset);
      break;
    case SCROLL_DOWN:
      new_origin.Offset(0, kMoveOffset);
      break;
  }
  RedrawDIP(new_origin, scale_, true);
}

void MagnificationControllerImpl::OnMouseMove(const gfx::Point& location) {
  DCHECK(root_window_);

  gfx::Point mouse(location);

  int x = origin_.x();
  int y = origin_.y();
  bool start_zoom = false;

  const gfx::Rect window_rect = gfx::ToEnclosingRect(GetWindowRectDIP(scale_));
  const int left = window_rect.x();
  const int right = window_rect.right();
  int margin = kPanningMergin / scale_;  // No need to consider DPI.

  int x_diff = 0;

  if (mouse.x() < left + margin) {
    // Panning left.
    x_diff = mouse.x() - (left + margin);
    start_zoom = true;
  } else if (right - margin < mouse.x()) {
    // Panning right.
    x_diff = mouse.x() - (right - margin);
    start_zoom = true;
  }
  x = left + x_diff;

  const int top = window_rect.y();
  const int bottom = window_rect.bottom();

  int y_diff = 0;
  if (mouse.y() < top + margin) {
    // Panning up.
    y_diff = mouse.y() - (top + margin);
    start_zoom = true;
  } else if (bottom - margin < mouse.y()) {
    // Panning down.
    y_diff = mouse.y() - (bottom - margin);
    start_zoom = true;
  }
  y = top + y_diff;

  if (start_zoom && !is_on_animation_) {
    // No animation on panning.
    bool animate = false;
    bool ret = RedrawDIP(gfx::Point(x, y), scale_, animate);

    if (ret) {
      // If the magnified region is moved, hides the mouse cursor and moves it.
      if (x_diff != 0 || y_diff != 0)
        MoveCursorTo(root_window_->GetHost(), mouse);
    }
  }
}

void MagnificationControllerImpl::AfterAnimationMoveCursorTo(
    const gfx::Point& location) {
  DCHECK(root_window_);

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window_);
  if (cursor_client) {
    // When cursor is invisible, do not move or show the cursor after the
    // animation.
    if (!cursor_client->IsCursorVisible())
      return;
    cursor_client->DisableMouseEvents();
  }
  move_cursor_after_animation_ = true;
  position_after_animation_ = location;
}

gfx::Size MagnificationControllerImpl::GetHostSizeDIP() const {
  return root_window_->bounds().size();
}

gfx::RectF MagnificationControllerImpl::GetWindowRectDIP(float scale) const {
  const gfx::Size size_in_dip = root_window_->bounds().size();
  const float width = size_in_dip.width() / scale;
  const float height = size_in_dip.height() / scale;

  return gfx::RectF(origin_.x(), origin_.y(), width, height);
}

bool MagnificationControllerImpl::IsMagnified() const {
  return scale_ >= kMinMagnifiedScaleThreshold;
}

void MagnificationControllerImpl::ValidateScale(float* scale) {
  // Adjust the scale to just |kNonMagnifiedScale| if scale is smaller than
  // |kMinMagnifiedScaleThreshold|;
  if (*scale < kMinMagnifiedScaleThreshold)
    *scale = kNonMagnifiedScale;

  // Adjust the scale to just |kMinMagnifiedScale| if scale is bigger than
  // |kMinMagnifiedScaleThreshold|;
  if (*scale > kMaxMagnifiedScaleThreshold)
    *scale = kMaxMagnifiedScale;

  DCHECK(kNonMagnifiedScale <= *scale && *scale <= kMaxMagnifiedScale);
}

void MagnificationControllerImpl::OnImplicitAnimationsCompleted() {
  if (!is_on_animation_)
    return;

  if (move_cursor_after_animation_) {
    MoveCursorTo(root_window_->GetHost(), position_after_animation_);
    move_cursor_after_animation_ = false;

    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(root_window_);
    if (cursor_client)
      cursor_client->EnableMouseEvents();
  }

  is_on_animation_ = false;

  StartOrStopScrollIfNecessary();
}

void MagnificationControllerImpl::OnWindowDestroying(
    aura::Window* root_window) {
  if (root_window == root_window_) {
    // There must be at least one root window because this controller is
    // destroyed before the root windows get destroyed.
    DCHECK(root_window);

    aura::Window* target_root_window = Shell::GetTargetRootWindow();
    CHECK(target_root_window);

    // The destroyed root window must not be target.
    CHECK_NE(target_root_window, root_window);
    // Don't redraw the old root window as it's being destroyed.
    SwitchTargetRootWindow(target_root_window, false);
    point_of_interest_ = target_root_window->bounds().CenterPoint();
  }
}

void MagnificationControllerImpl::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  // TODO(yoshiki): implement here. crbug.com/230979
}

void MagnificationControllerImpl::SwitchTargetRootWindow(
    aura::Window* new_root_window,
    bool redraw_original_root_window) {
  DCHECK(new_root_window);

  if (new_root_window == root_window_)
    return;

  // Stores the previous scale.
  float scale = GetScale();

  // Unmagnify the previous root window.
  root_window_->RemoveObserver(this);
  if (redraw_original_root_window)
    RedrawKeepingMousePosition(1.0f, true);

  root_window_ = new_root_window;
  RedrawKeepingMousePosition(scale, true);
  root_window_->AddObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// MagnificationControllerImpl: MagnificationController implementation

void MagnificationControllerImpl::SetScale(float scale, bool animate) {
  if (!is_enabled_)
    return;

  ValidateScale(&scale);
  Shell::GetInstance()->accessibility_delegate()->
      SaveScreenMagnifierScale(scale);
  RedrawKeepingMousePosition(scale, animate);
}

void MagnificationControllerImpl::MoveWindow(int x, int y, bool animate) {
  if (!is_enabled_)
    return;

  Redraw(gfx::Point(x, y), scale_, animate);
}

void MagnificationControllerImpl::MoveWindow(const gfx::Point& point,
                                             bool animate) {
  if (!is_enabled_)
    return;

  Redraw(point, scale_, animate);
}

void MagnificationControllerImpl::SetScrollDirection(
    ScrollDirection direction) {
  scroll_direction_ = direction;
  StartOrStopScrollIfNecessary();
}

void MagnificationControllerImpl::SetEnabled(bool enabled) {
  Shell* shell = Shell::GetInstance();
  if (enabled) {
    float scale =
        Shell::GetInstance()->accessibility_delegate()->
        GetSavedScreenMagnifierScale();
    if (scale <= 0.0f)
      scale = kInitialMagnifiedScale;
    ValidateScale(&scale);

    // Do nothing, if already enabled with same scale.
    if (is_enabled_ && scale == scale_)
      return;

    is_enabled_ = enabled;
    RedrawKeepingMousePosition(scale, true);
    shell->accessibility_delegate()->SaveScreenMagnifierScale(scale);
  } else {
    // Do nothing, if already disabled.
    if (!is_enabled_)
      return;

    RedrawKeepingMousePosition(kNonMagnifiedScale, true);
    is_enabled_ = enabled;
  }
}

bool MagnificationControllerImpl::IsEnabled() const {
  return is_enabled_;
}

////////////////////////////////////////////////////////////////////////////////
// MagnificationControllerImpl: aura::EventFilter implementation

void MagnificationControllerImpl::OnMouseEvent(ui::MouseEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* current_root = target->GetRootWindow();
  gfx::Rect root_bounds = current_root->bounds();

  if (root_bounds.Contains(event->root_location())) {
    // This must be before |SwitchTargetRootWindow()|.
    if (event->type() != ui::ET_MOUSE_CAPTURE_CHANGED)
      point_of_interest_ = event->root_location();

    if (current_root != root_window_) {
      DCHECK(current_root);
      SwitchTargetRootWindow(current_root, true);
    }

    if (IsMagnified() && event->type() == ui::ET_MOUSE_MOVED)
      OnMouseMove(event->root_location());
  }
}

void MagnificationControllerImpl::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->IsAltDown() && event->IsControlDown()) {
    if (event->type() == ui::ET_SCROLL_FLING_START ||
        event->type() == ui::ET_SCROLL_FLING_CANCEL) {
      event->StopPropagation();
      return;
    }

    if (event->type() == ui::ET_SCROLL) {
      ui::ScrollEvent* scroll_event = static_cast<ui::ScrollEvent*>(event);
      float scale = GetScale();
      scale += scroll_event->y_offset() * kScrollScaleChangeFactor;
      SetScale(scale, true);
      event->StopPropagation();
      return;
    }
  }
}

void MagnificationControllerImpl::OnTouchEvent(ui::TouchEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* current_root = target->GetRootWindow();
  if (current_root == root_window_) {
    gfx::Rect root_bounds = current_root->bounds();
    if (root_bounds.Contains(event->root_location()))
      point_of_interest_ = event->root_location();
  }
}

////////////////////////////////////////////////////////////////////////////////
// MagnificationController:

// static
MagnificationController* MagnificationController::CreateInstance() {
  return new MagnificationControllerImpl();
}

}  // namespace ash
