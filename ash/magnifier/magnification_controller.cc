// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnification_controller.h"

#include "ash/shell.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/compound_event_filter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/base/events/event.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/point3.h"
#include "ui/gfx/screen.h"

namespace {

const float kMaxMagnifiedScale = 4.0f;
const float kMaxMagnifiedScaleThreshold = 4.0f;
const float kMinMagnifiedScaleThreshold = 1.1f;
const float kNonMagnifiedScale = 1.0f;

const float kInitialMagnifiedScale = 2.0f;
const float kScrollScaleChangeFactor = 0.05f;

}  // namespace

namespace ash {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// MagnificationControllerImpl:

class MagnificationControllerImpl : virtual public MagnificationController,
                                    public aura::EventFilter,
                                    public ui::ImplicitAnimationObserver {
 public:
  MagnificationControllerImpl();
  virtual ~MagnificationControllerImpl();

  // MagnificationController overrides:
  virtual void SetEnabled(bool enabled) OVERRIDE;
  virtual void SetScale(float scale, bool animate) OVERRIDE;
  virtual float GetScale() const OVERRIDE { return scale_; }
  virtual void MoveWindow(int x, int y, bool animate) OVERRIDE;
  virtual void MoveWindow(const gfx::Point& point, bool animate) OVERRIDE;
  virtual gfx::Point GetWindowPosition() const OVERRIDE { return origin_; }
  virtual void EnsureRectIsVisible(const gfx::Rect& rect,
                                   bool animate) OVERRIDE;
  virtual void EnsurePointIsVisible(const gfx::Point& point,
                                    bool animate) OVERRIDE;

 private:
  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  // Redraws the magnification window with the given origin position and the
  // given scale. Returns true if the window is changed; otherwise, false.
  // These methods should be called internally just after the scale and/or
  // the position are changed to redraw the window.
  bool Redraw(const gfx::Point& position, float scale, bool animate);
  bool RedrawDIP(const gfx::Point& position, float scale, bool animate);

  // Ensures that the given point, rect or last mouse location is inside
  // magnification window. If not, the controller moves the window to contain
  // the given point/rect.
  void EnsureRectIsVisibleWithScale(const gfx::Rect& target_rect,
                                    float scale,
                                    bool animate);
  void EnsureRectIsVisibleDIP(const gfx::Rect& target_rect_in_dip,
                              float scale,
                              bool animate);
  void EnsurePointIsVisibleWithScale(const gfx::Point& point,
                                     float scale,
                                     bool animate);
  void OnMouseMove(const gfx::Point& location);

  // Switch Magnified RootWindow to |new_root_window|. This does following:
  //  - Unzoom the current root_window.
  //  - Zoom the given new root_window |new_root_window|.
  //  - Switch the target window from current window to |new_root_window|.
  void SwitchTargetRootWindow(aura::RootWindow* new_root_window);

  // Returns if the magnification scale is 1.0 or not (larger then 1.0).
  bool IsMagnified() const;

  // Returns the rect of the magnification window.
  gfx::Rect GetWindowRectDIP(float scale) const;
  // Returns the size of the root window.
  gfx::Size GetHostSizeDIP() const;

  // Correct the givin scale value if nessesary.
  void ValidateScale(float* scale);

  // aura::EventFilter overrides:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 ui::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   ui::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(
      aura::Window* target,
      ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult PreHandleGestureEvent(
      aura::Window* target,
      ui::GestureEvent* event) OVERRIDE;

  aura::RootWindow* root_window_;

  // True if the magnified window is in motion of zooming or un-zooming effect.
  // Otherwise, false.
  bool is_on_zooming_;

  bool is_enabled_;

  // Current scale, origin (left-top) position of the magnification window.
  float scale_;
  gfx::Point origin_;

  DISALLOW_COPY_AND_ASSIGN(MagnificationControllerImpl);
};

////////////////////////////////////////////////////////////////////////////////
// MagnificationControllerImpl:

MagnificationControllerImpl::MagnificationControllerImpl()
    : root_window_(ash::Shell::GetPrimaryRootWindow()),
      is_on_zooming_(false),
      is_enabled_(false),
      scale_(kNonMagnifiedScale) {
  Shell::GetInstance()->AddEnvEventFilter(this);
}

MagnificationControllerImpl::~MagnificationControllerImpl() {
  Shell::GetInstance()->RemoveEnvEventFilter(this);
}

bool MagnificationControllerImpl::Redraw(const gfx::Point& position,
                                         float scale,
                                         bool animate) {
  const gfx::Point position_in_dip =
      ui::ConvertPointToDIP(root_window_->layer(), position);
  return RedrawDIP(position_in_dip, scale, animate);
}

bool MagnificationControllerImpl::RedrawDIP(const gfx::Point& position_in_dip,
                                            float scale,
                                            bool animate) {
  int x = position_in_dip.x();
  int y = position_in_dip.y();

  ValidateScale(&scale);

  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;

  const gfx::Size host_size_in_dip = GetHostSizeDIP();
  const gfx::Size window_size_in_dip = GetWindowRectDIP(scale).size();
  int max_x = host_size_in_dip.width() - window_size_in_dip.width();
  int max_y = host_size_in_dip.height() - window_size_in_dip.height();
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
  ui::Transform transform;
  // Flips the signs intentionally to convert them from the position of the
  // magnification window.
  transform.ConcatTranslate(-origin_.x(), -origin_.y());
  transform.ConcatScale(scale_, scale_);

  ui::ScopedLayerAnimationSettings settings(
      root_window_->layer()->GetAnimator());
  settings.AddObserver(this);
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTweenType(ui::Tween::EASE_OUT);
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(animate ? 100 : 0));

  root_window_->layer()->SetTransform(transform);

  return true;
}

void MagnificationControllerImpl::EnsureRectIsVisibleWithScale(
    const gfx::Rect& target_rect,
    float scale,
    bool animate) {
  const gfx::Rect target_rect_in_dip =
      ui::ConvertRectToDIP(root_window_->layer(), target_rect);
  EnsureRectIsVisibleDIP(target_rect_in_dip, scale, animate);
}

void MagnificationControllerImpl::EnsureRectIsVisibleDIP(
    const gfx::Rect& target_rect,
    float scale,
    bool animate) {
  ValidateScale(&scale);

  const gfx::Rect window_rect = GetWindowRectDIP(scale);
  if (scale == scale_ && window_rect.Contains(target_rect))
    return;

  // TODO(yoshiki): Un-zoom and change the scale if the magnification window
  // can't contain the whole given rect.

  gfx::Rect rect = window_rect;
  if (target_rect.width() > rect.width())
    rect.set_x(target_rect.CenterPoint().x() - rect.x() / 2);
  else if (target_rect.right() < rect.x())
    rect.set_x(target_rect.right());
  else if (rect.right() < target_rect.x())
    rect.set_x(target_rect.x() - rect.width());

  if (rect.height() > window_rect.height())
    rect.set_y(target_rect.CenterPoint().y() - rect.y() / 2);
  else if (target_rect.bottom() < rect.y())
    rect.set_y(target_rect.bottom());
  else if (rect.bottom() < target_rect.y())
    rect.set_y(target_rect.y() - rect.height());

  RedrawDIP(rect.origin(), scale, animate);
}

void MagnificationControllerImpl::EnsurePointIsVisibleWithScale(
    const gfx::Point& point,
    float scale,
    bool animate) {
  EnsureRectIsVisibleWithScale(gfx::Rect(point, gfx::Size(0, 0)),
                               scale,
                               animate);
}

void MagnificationControllerImpl::OnMouseMove(const gfx::Point& location) {
  gfx::Point mouse(location);

  int x = origin_.x();
  int y = origin_.y();
  bool start_zoom = false;

  const gfx::Rect window_rect = GetWindowRectDIP(scale_);
  const int left = window_rect.x();
  const int right = window_rect.right();
  const int width_margin = static_cast<int>(0.1f * window_rect.width());
  const int width_offset = static_cast<int>(0.5f * window_rect.width());

  if (mouse.x() < left + width_margin) {
    x -= width_offset;
    start_zoom = true;
  } else if (right - width_margin < mouse.x()) {
    x += width_offset;
    start_zoom = true;
  }

  const int top = window_rect.y();
  const int bottom = window_rect.bottom();
  // Uses same margin with x-axis's one.
  const int height_margin = width_margin;
  const int height_offset = static_cast<int>(0.5f * window_rect.height());

  if (mouse.y() < top + height_margin) {
    y -= height_offset;
    start_zoom = true;
  } else if (bottom - height_margin < mouse.y()) {
    y += height_offset;
    start_zoom = true;
  }

  if (start_zoom && !is_on_zooming_) {
    bool ret = RedrawDIP(gfx::Point(x, y), scale_, true);

    if (ret) {
      is_on_zooming_ = true;

      int x_diff = origin_.x() - window_rect.x();
      int y_diff = origin_.y() - window_rect.y();
      // If the magnified region is moved, hides the mouse cursor and moves it.
      if (x_diff != 0 || y_diff != 0) {
        ash::Shell::GetInstance()->
            env_filter()->set_update_cursor_visibility(false);
        root_window_->ShowCursor(false);
        mouse.set_x(mouse.x() - (origin_.x() - window_rect.x()));
        mouse.set_y(mouse.y() - (origin_.y() - window_rect.y()));
        root_window_->MoveCursorTo(mouse);
      }
    }
  }
}

gfx::Size MagnificationControllerImpl::GetHostSizeDIP() const {
  return ui::ConvertSizeToDIP(root_window_->layer(),
                              root_window_->GetHostSize());
}

gfx::Rect MagnificationControllerImpl::GetWindowRectDIP(float scale) const {
  const gfx::Size size_in_dip =
      ui::ConvertSizeToDIP(root_window_->layer(),
                           root_window_->GetHostSize());
  const int width = size_in_dip.width() / scale;
  const int height = size_in_dip.height() / scale;

  return gfx::Rect(origin_.x(), origin_.y(), width, height);
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
}

void MagnificationControllerImpl::OnImplicitAnimationsCompleted() {
  root_window_->ShowCursor(true);
  is_on_zooming_ = false;
}

void MagnificationControllerImpl::SwitchTargetRootWindow(
    aura::RootWindow* new_root_window) {
  if (new_root_window == root_window_)
    return;

  float scale = GetScale();

  SetScale(1.0f, true);
  root_window_ = new_root_window;
  SetScale(scale, true);
}

////////////////////////////////////////////////////////////////////////////////
// MagnificationControllerImpl: MagnificationController implementation

void MagnificationControllerImpl::SetScale(float scale, bool animate) {
  if (!is_enabled_)
    return;

  ValidateScale(&scale);

  gfx::Point mouse_in_root = root_window_->GetLastMouseLocationInRoot();
  const gfx::Point origin =
      gfx::Point(mouse_in_root.x() * (1.0f - 1.0f / scale),
                 mouse_in_root.y() * (1.0f - 1.0f / scale));
  Redraw(origin, scale, animate);
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

void MagnificationControllerImpl::EnsureRectIsVisible(
    const gfx::Rect& target_rect,
    bool animate) {
  if (!is_enabled_)
    return;

  EnsureRectIsVisibleWithScale(target_rect, scale_, animate);
}

void MagnificationControllerImpl::EnsurePointIsVisible(
    const gfx::Point& point,
    bool animate) {
  if (!is_enabled_)
    return;

  EnsurePointIsVisibleWithScale(point, scale_, animate);
}

void MagnificationControllerImpl::SetEnabled(bool enabled) {
  if (enabled) {
    is_enabled_ = enabled;
    SetScale(kInitialMagnifiedScale, true);
  } else {
    SetScale(kNonMagnifiedScale, true);
    is_enabled_ = enabled;
  }
}

////////////////////////////////////////////////////////////////////////////////
// MagnificationControllerImpl: aura::EventFilter implementation

bool MagnificationControllerImpl::PreHandleKeyEvent(aura::Window* target,
                                                    ui::KeyEvent* event) {
  return false;
}

bool MagnificationControllerImpl::PreHandleMouseEvent(aura::Window* target,
                                                      ui::MouseEvent* event) {
  if (event->type() == ui::ET_SCROLL && event->IsAltDown()) {
    ui::ScrollEvent* scroll_event = static_cast<ui::ScrollEvent*>(event);
    float scale = GetScale();
    scale += scroll_event->y_offset() * kScrollScaleChangeFactor;
    SetScale(scale, true);
    return true;
  }

  if (IsMagnified() && event->type() == ui::ET_MOUSE_MOVED) {
    aura::RootWindow* current_root = target->GetRootWindow();
    gfx::Rect root_bounds = current_root->bounds();

    if (root_bounds.Contains(event->root_location())) {
      if (current_root != root_window_)
        SwitchTargetRootWindow(current_root);

      OnMouseMove(event->root_location());
    }
  }

  return false;
}

ui::TouchStatus MagnificationControllerImpl::PreHandleTouchEvent(
    aura::Window* target,
    ui::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult MagnificationControllerImpl::PreHandleGestureEvent(
    aura::Window* target,
    ui::GestureEvent* event) {
  return ui::ER_UNHANDLED;
}

////////////////////////////////////////////////////////////////////////////////
// MagnificationController:

// static
MagnificationController* MagnificationController::CreateInstance() {
  return new MagnificationControllerImpl();
}

}  // namespace internal
}  // namespace ash
