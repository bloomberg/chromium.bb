// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnification_controller.h"

#include <memory>
#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accessibility_delegate.h"
#include "ash/ash_switches.h"
#include "ash/display/root_window_transformers.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/root_window_transformer.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/command_line.h"
#include "base/synchronization/waitable_event.h"
#include "base/timer/timer.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace {

const float kMaxMagnifiedScale = 4.0f;
const float kMaxMagnifiedScaleThreshold = 4.0f;
const float kMinMagnifiedScaleThreshold = 1.1f;
const float kNonMagnifiedScale = 1.0f;

const float kInitialMagnifiedScale = 2.0f;
const float kScrollScaleChangeFactor = 0.0125f;

// Default animation parameters for redrawing the magnification window.
const gfx::Tween::Type kDefaultAnimationTweenType = gfx::Tween::EASE_OUT;
const int kDefaultAnimationDurationInMs = 100;

// Use linear transformation to make the magnifier window move smoothly
// to center the focus when user types in a text input field.
const gfx::Tween::Type kCenterCaretAnimationTweenType = gfx::Tween::LINEAR;

// The delay of the timer for moving magnifier window for centering the text
// input focus.
const int kMoveMagnifierDelayInMs = 10;

// Threadshold of panning. If the cursor moves to within pixels (in DIP) of
// |kCursorPanningMargin| from the edge, the view-port moves.
const int kCursorPanningMargin = 100;

// Threadshold of panning. If the caret moves to within pixels (in DIP) of
// |kCaretPanningMargin| from the edge, the view-port moves.
const int kCaretPanningMargin = 50;

void MoveCursorTo(aura::WindowTreeHost* host, const gfx::Point& root_location) {
  auto host_location_3f = gfx::Point3F(gfx::PointF(root_location));
  host->GetRootTransform().TransformPoint(&host_location_3f);
  host->MoveCursorToHostLocation(
      gfx::ToCeiledPoint(host_location_3f.AsPointF()));
}

ui::InputMethod* GetInputMethod(aura::Window* root_window) {
  if (root_window->GetHost())
    return root_window->GetHost()->GetInputMethod();
  return nullptr;
}

}  // namespace

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// MagnificationControllerImpl:

class MagnificationControllerImpl : public MagnificationController,
                                    public ui::EventHandler,
                                    public ui::ImplicitAnimationObserver,
                                    public aura::WindowObserver,
                                    public ui::InputMethodObserver {
 public:
  MagnificationControllerImpl();
  ~MagnificationControllerImpl() override;

  // MagnificationController overrides:
  void SetEnabled(bool enabled) override;
  bool IsEnabled() const override;
  void SetKeepFocusCentered(bool keep_focus_centered) override;
  bool KeepFocusCentered() const override;
  void SetScale(float scale, bool animate) override;
  float GetScale() const override { return scale_; }
  void MoveWindow(int x, int y, bool animate) override;
  void MoveWindow(const gfx::Point& point, bool animate) override;
  gfx::Point GetWindowPosition() const override {
    return gfx::ToFlooredPoint(origin_);
  }
  void SetScrollDirection(ScrollDirection direction) override;
  gfx::Rect GetViewportRect() const override;
  void HandleFocusedNodeChanged(
      bool is_editable_node,
      const gfx::Rect& node_bounds_in_screen) override;
  void SwitchTargetRootWindow(aura::Window* new_root_window,
                              bool redraw_original_root_window) override;

  // For test
  gfx::Point GetPointOfInterestForTesting() override {
    return point_of_interest_;
  }

  bool IsOnAnimationForTesting() const override { return is_on_animation_; }

  void DisableMoveMagnifierDelayForTesting() override {
    disable_move_magnifier_delay_ = true;
  }

 private:
  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;

  // aura::WindowObserver overrides:
  void OnWindowDestroying(aura::Window* root_window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;

  // Redraws the magnification window with the given origin position and the
  // given scale. Returns true if the window is changed; otherwise, false.
  // These methods should be called internally just after the scale and/or
  // the position are changed to redraw the window.
  bool Redraw(const gfx::PointF& position, float scale, bool animate);

  // Redraws the magnification window with the given origin position in dip and
  // the given scale. Returns true if the window is changed; otherwise, false.
  // The last two parameters specify the animation duration and tween type.
  // If |animation_in_ms| is zero, there will be no animation, and |tween_type|
  // will be ignored.
  bool RedrawDIP(const gfx::PointF& position_in_dip,
                 float scale,
                 int animation_in_ms,
                 gfx::Tween::Type tween_type);

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

  // Returns if the magnification scale is 1.0 or not (larger then 1.0).
  bool IsMagnified() const;

  // Returns the rect of the magnification window.
  gfx::RectF GetWindowRectDIP(float scale) const;
  // Returns the size of the root window.
  gfx::Size GetHostSizeDIP() const;

  // Correct the given scale value if necessary.
  void ValidateScale(float* scale);

  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // Moves the view port when |point| is located within
  // |x_panning_margin| and |y_pannin_margin| to the edge of the visible
  // window region. The view port will be moved so that the |point| will be
  // moved to the point where it has |x_target_margin| and |y_target_margin|
  // to the edge of the visible region.
  void MoveMagnifierWindowFollowPoint(const gfx::Point& point,
                                      int x_panning_margin,
                                      int y_panning_margin,
                                      int x_target_margin,
                                      int y_target_margin);

  // Moves the view port to center |point| in magnifier screen.
  void MoveMagnifierWindowCenterPoint(const gfx::Point& point);

  // Moves the viewport so that |rect| is fully visible. If |rect| is larger
  // than the viewport horizontally or vertically, the viewport will be moved
  // to center the |rect| in that dimension.
  void MoveMagnifierWindowFollowRect(const gfx::Rect& rect);

  // Invoked when |move_magnifier_timer_| fires to move the magnifier window to
  // follow the caret.
  void OnMoveMagnifierTimer();

  // ui::InputMethodObserver:
  void OnTextInputTypeChanged(const ui::TextInputClient* client) override {}
  void OnFocus() override {}
  void OnBlur() override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}
  void OnShowImeIfNeeded() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;

  // Target root window. This must not be NULL.
  aura::Window* root_window_;

  // True if the magnified window is currently animating a change. Otherwise,
  // false.
  bool is_on_animation_;

  bool is_enabled_;

  bool keep_focus_centered_;

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

  // Timer for moving magnifier window when it fires.
  base::OneShotTimer move_magnifier_timer_;

  // Most recent caret position in |root_window_| coordinates.
  gfx::Point caret_point_;

  // Flag for disabling moving magnifier delay. It can only be true in testing
  // mode.
  bool disable_move_magnifier_delay_;

  DISALLOW_COPY_AND_ASSIGN(MagnificationControllerImpl);
};

////////////////////////////////////////////////////////////////////////////////
// MagnificationControllerImpl:

MagnificationControllerImpl::MagnificationControllerImpl()
    : root_window_(Shell::GetPrimaryRootWindow()),
      is_on_animation_(false),
      is_enabled_(false),
      keep_focus_centered_(false),
      move_cursor_after_animation_(false),
      scale_(kNonMagnifiedScale),
      scroll_direction_(SCROLL_NONE),
      disable_move_magnifier_delay_(false) {
  Shell::GetInstance()->AddPreTargetHandler(this);
  root_window_->AddObserver(this);
  point_of_interest_ = root_window_->bounds().CenterPoint();
}

MagnificationControllerImpl::~MagnificationControllerImpl() {
  ui::InputMethod* input_method = GetInputMethod(root_window_);
  if (input_method)
    input_method->RemoveObserver(this);

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
  bool changed = RedrawDIP(origin, scale,
                           animate ? kDefaultAnimationDurationInMs : 0,
                           kDefaultAnimationTweenType);
  if (changed)
    AfterAnimationMoveCursorTo(mouse_in_root);
}

bool MagnificationControllerImpl::Redraw(const gfx::PointF& position,
                                         float scale,
                                         bool animate) {
  const gfx::PointF position_in_dip =
      ui::ConvertPointToDIP(root_window_->layer(), position);
  return RedrawDIP(position_in_dip, scale,
                   animate ? kDefaultAnimationDurationInMs : 0,
                   kDefaultAnimationTweenType);
}

bool MagnificationControllerImpl::RedrawDIP(const gfx::PointF& position_in_dip,
                                            float scale,
                                            int duration_in_ms,
                                            gfx::Tween::Type tween_type) {
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
  settings.SetTweenType(tween_type);
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(duration_in_ms));

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window_);
  std::unique_ptr<RootWindowTransformer> transformer(
      CreateRootWindowTransformerForDisplay(root_window_, display));
  GetRootWindowController(root_window_)
      ->ash_host()
      ->SetRootWindowTransformer(std::move(transformer));

  if (duration_in_ms > 0)
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
  RedrawDIP(new_origin, scale_, kDefaultAnimationDurationInMs,
            kDefaultAnimationTweenType);
}

void MagnificationControllerImpl::OnMouseMove(const gfx::Point& location) {
  DCHECK(root_window_);

  gfx::Point mouse(location);
  int margin = kCursorPanningMargin / scale_;  // No need to consider DPI.
  MoveMagnifierWindowFollowPoint(mouse, margin, margin, margin, margin);
}

gfx::Rect MagnificationControllerImpl::GetViewportRect() const {
  return gfx::ToEnclosingRect(GetWindowRectDIP(scale_));
}

void MagnificationControllerImpl::HandleFocusedNodeChanged(
    bool is_editable_node,
    const gfx::Rect& node_bounds_in_screen) {
  // The editable node is handled by OnCaretBoundsChanged.
  if (is_editable_node)
    return;

  // Nothing to recenter on.
  if (node_bounds_in_screen.IsEmpty())
    return;

  gfx::Rect node_bounds_in_root =
      ScreenUtil::ConvertRectFromScreen(root_window_, node_bounds_in_screen);
  if (GetViewportRect().Contains(node_bounds_in_root))
    return;

  MoveMagnifierWindowFollowRect(node_bounds_in_root);
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

  Redraw(gfx::PointF(x, y), scale_, animate);
}

void MagnificationControllerImpl::MoveWindow(const gfx::Point& point,
                                             bool animate) {
  if (!is_enabled_)
    return;

  Redraw(gfx::PointF(point), scale_, animate);
}

void MagnificationControllerImpl::SetScrollDirection(
    ScrollDirection direction) {
  scroll_direction_ = direction;
  StartOrStopScrollIfNecessary();
}

void MagnificationControllerImpl::SetEnabled(bool enabled) {
  Shell* shell = Shell::GetInstance();
  ui::InputMethod* input_method = GetInputMethod(root_window_);
  if (enabled) {
    if (!is_enabled_ && input_method)
      input_method->AddObserver(this);

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

    if (input_method)
      input_method->RemoveObserver(this);

    RedrawKeepingMousePosition(kNonMagnifiedScale, true);
    is_enabled_ = enabled;
  }
}

bool MagnificationControllerImpl::IsEnabled() const {
  return is_enabled_;
}

void MagnificationControllerImpl::SetKeepFocusCentered(
    bool keep_focus_centered) {
  keep_focus_centered_ = keep_focus_centered;
}

bool MagnificationControllerImpl::KeepFocusCentered() const {
  return keep_focus_centered_;
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
      ui::ScrollEvent* scroll_event = event->AsScrollEvent();
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

void MagnificationControllerImpl::MoveMagnifierWindowFollowPoint(
    const gfx::Point& point,
    int x_panning_margin,
    int y_panning_margin,
    int x_target_margin,
    int y_target_margin) {
  DCHECK(root_window_);
  bool start_zoom = false;

  const gfx::Rect window_rect = GetViewportRect();
  const int left = window_rect.x();
  const int right = window_rect.right();

  int x_diff = 0;
  if (point.x() < left + x_panning_margin) {
    // Panning left.
    x_diff = point.x() - (left + x_target_margin);
    start_zoom = true;
  } else if (right - x_panning_margin < point.x()) {
    // Panning right.
    x_diff = point.x() - (right - x_target_margin);
    start_zoom = true;
  }
  int x = left + x_diff;

  const int top = window_rect.y();
  const int bottom = window_rect.bottom();

  int y_diff = 0;
  if (point.y() < top + y_panning_margin) {
    // Panning up.
    y_diff = point.y() - (top + y_target_margin);
    start_zoom = true;
  } else if (bottom - y_panning_margin < point.y()) {
    // Panning down.
    y_diff = point.y() - (bottom - y_target_margin);
    start_zoom = true;
  }
  int y = top + y_diff;
  if (start_zoom && !is_on_animation_) {
    bool ret = RedrawDIP(gfx::PointF(x, y), scale_,
                         0,  // No animation on panning.
                         kDefaultAnimationTweenType);

    if (ret) {
      // If the magnified region is moved, hides the mouse cursor and moves it.
      if (x_diff != 0 || y_diff != 0)
        MoveCursorTo(root_window_->GetHost(), point);
    }
  }
}

void MagnificationControllerImpl::MoveMagnifierWindowCenterPoint(
    const gfx::Point& point) {
  DCHECK(root_window_);

  const gfx::Rect window_rect = GetViewportRect();
  if (point == window_rect.CenterPoint())
    return;

  if (!is_on_animation_) {
    // With animation on panning.
    RedrawDIP(
        gfx::PointF(window_rect.origin() + (point - window_rect.CenterPoint())),
        scale_, kDefaultAnimationDurationInMs, kCenterCaretAnimationTweenType);
  }
}

void MagnificationControllerImpl::MoveMagnifierWindowFollowRect(
    const gfx::Rect& rect) {
  DCHECK(root_window_);
  bool should_pan = false;

  const gfx::Rect viewport_rect = GetViewportRect();
  const int left = viewport_rect.x();
  const int right = viewport_rect.right();
  const gfx::Point rect_center = rect.CenterPoint();
  const gfx::Point window_center = viewport_rect.CenterPoint();

  int x = left;
  if (rect.x() < left || right < rect.right()) {
    // Panning horizontally.
    x = rect_center.x() - viewport_rect.width() / 2;
    should_pan = true;
  }

  const int top = viewport_rect.y();
  const int bottom = viewport_rect.bottom();

  int y = top;
  if (rect.y() < top || bottom < rect.bottom()) {
    // Panning vertically.
    y = rect_center.y() - viewport_rect.height() / 2;
    should_pan = true;
  }

  if (should_pan) {
    if (is_on_animation_) {
      root_window_->layer()->GetAnimator()->StopAnimating();
      is_on_animation_ = false;
    }
    RedrawDIP(gfx::PointF(x, y), scale_,
              0,  // No animation on panning.
              kDefaultAnimationTweenType);
  }
}

void MagnificationControllerImpl::OnMoveMagnifierTimer() {
  MoveMagnifierWindowCenterPoint(caret_point_);
}

void MagnificationControllerImpl::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
  // caret bounds in screen coordinates.
  const gfx::Rect caret_bounds = client->GetCaretBounds();
  // Note: OnCaretBoundsChanged could be fired OnTextInputTypeChanged during
  // which the caret position is not set a meaning position, and we do not
  // need to adjust the view port position based on the bogus caret position.
  // This is only a transition period, the caret position will be fixed upon
  // focusing right after.
  if (caret_bounds.width() == 0 && caret_bounds.height() == 0)
    return;

  caret_point_ = caret_bounds.CenterPoint();
  // |caret_point_| in |root_window_| coordinates.
  ::wm::ConvertPointFromScreen(root_window_, &caret_point_);

  // If the feature for centering the text input focus is disabled, the
  // magnifier window will be moved to follow the focus with a panning margin.
  if (!KeepFocusCentered()) {
    // Visible window_rect in |root_window_| coordinates.
    const gfx::Rect visible_window_rect = GetViewportRect();
    const int panning_margin = kCaretPanningMargin / scale_;
    MoveMagnifierWindowFollowPoint(caret_point_,
                                   panning_margin,
                                   panning_margin,
                                   visible_window_rect.width() / 2,
                                   visible_window_rect.height() / 2);
    return;
  }

  // Move the magnifier window to center the focus with a little delay.
  // In Gmail compose window, when user types a blank space, it will insert
  // a non-breaking space(NBSP). NBSP will be replaced with a blank space
  // character when user types a non-blank space character later, which causes
  // OnCaretBoundsChanged be called twice. The first call moves the caret back
  // to the character position just before NBSP, replaces the NBSP with blank
  // space plus the new character, then the second call will move caret to the
  // position after the new character. In order to avoid the magnifier window
  // being moved back and forth with these two OnCaretBoundsChanged events, we
  // defer moving magnifier window until the |move_magnifier_timer_| fires,
  // when the caret settles eventually.
  move_magnifier_timer_.Stop();
  move_magnifier_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(
          disable_move_magnifier_delay_ ? 0 : kMoveMagnifierDelayInMs),
      this, &MagnificationControllerImpl::OnMoveMagnifierTimer);
}

////////////////////////////////////////////////////////////////////////////////
// MagnificationController:

// static
MagnificationController* MagnificationController::CreateInstance() {
  return new MagnificationControllerImpl();
}

}  // namespace ash
