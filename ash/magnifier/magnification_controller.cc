// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnification_controller.h"

#include "ash/shell.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/compound_event_filter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/gfx/point3.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace {

const float kMaximumMagnifiScale = 4.0f;
const float kMaximumMagnifiScaleThreshold = 4.0f;
const float kMinimumMagnifiScale = 1.0f;
const float kMinimumMagnifiScaleThreshold = 1.1f;

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
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

  aura::RootWindow* root_window_;

  // True if the magnified window is in motion of zooming or un-zooming effect.
  // Otherwise, false.
  bool is_on_zooming_;

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
      scale_(1.0f) {
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

  // Ignores 1 px diffirence because it may be error on calculation.
  if (std::abs(origin_.x() - x) <= 1 &&
      std::abs(origin_.y() - y) <= 1 &&
      scale == scale_)
    return false;

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
    bool ret = Redraw(gfx::Point(x, y), scale_, true);

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
  return scale_ >= kMinimumMagnifiScaleThreshold;
}

void MagnificationControllerImpl::ValidateScale(float* scale) {
  // Adjust the scale to just |kMinimumMagnifiScale| if scale is smaller than
  // |kMinimumMagnifiScaleThreshold|;
  if (*scale < kMinimumMagnifiScaleThreshold)
    *scale = kMinimumMagnifiScale;

  // Adjust the scale to just |kMinimumMagnifiScale| if scale is bigger than
  // |kMinimumMagnifiScaleThreshold|;
  if (*scale > kMaximumMagnifiScaleThreshold)
    *scale = kMaximumMagnifiScale;
}

void MagnificationControllerImpl::OnImplicitAnimationsCompleted() {
  root_window_->ShowCursor(true);
  is_on_zooming_ = false;
}

////////////////////////////////////////////////////////////////////////////////
// MagnificationControllerImpl: MagnificationController implementation

void MagnificationControllerImpl::SetScale(float scale, bool animate) {
  ValidateScale(&scale);

  // Try not to change the point which the mouse cursor indicates to.
  const gfx::Rect window_rect = GetWindowRectDIP(scale);
  const gfx::Point mouse = root_window_->last_mouse_location();
  const gfx::Point origin = gfx::Point(mouse.x() * (1.0f - 1.0f / scale),
                                       mouse.y() * (1.0f - 1.0f / scale));
  Redraw(origin, scale, animate);
}

void MagnificationControllerImpl::MoveWindow(int x, int y, bool animate) {
  Redraw(gfx::Point(x, y), scale_, animate);
}

void MagnificationControllerImpl::MoveWindow(const gfx::Point& point,
                                             bool animate) {
  Redraw(point, scale_, animate);
}

void MagnificationControllerImpl::EnsureRectIsVisible(
    const gfx::Rect& target_rect,
    bool animate) {
  EnsureRectIsVisibleWithScale(target_rect, scale_, animate);
}

void MagnificationControllerImpl::EnsurePointIsVisible(
    const gfx::Point& point,
    bool animate) {
  EnsurePointIsVisibleWithScale(point, scale_, animate);
}

////////////////////////////////////////////////////////////////////////////////
// MagnificationControllerImpl: aura::EventFilter implementation

bool MagnificationControllerImpl::PreHandleKeyEvent(aura::Window* target,
                                                    aura::KeyEvent* event) {
  return false;
}

bool MagnificationControllerImpl::PreHandleMouseEvent(aura::Window* target,
                                                      aura::MouseEvent* event) {
  if (IsMagnified())
    OnMouseMove(event->root_location());
  return false;
}

ui::TouchStatus MagnificationControllerImpl::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus MagnificationControllerImpl::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
// MagnificationController:

// static
MagnificationController* MagnificationController::CreateInstance() {
  return new MagnificationControllerImpl();
}

}  // namespace internal
}  // namespace ash
