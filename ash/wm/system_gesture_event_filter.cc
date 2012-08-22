// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_gesture_event_filter.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/display/display_controller.h"
#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/brightness/brightness_control_delegate.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/volume_control_delegate.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "base/command_line.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/root_window.h"
#include "ui/base/event.h"
#include "ui/base/gestures/gesture_configuration.h"
#include "ui/base/gestures/gesture_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_CHROMEOS)
#include "ui/base/touch/touch_factory.h"
#endif

namespace {
using views::Widget;

const int kSystemGesturePoints = 4;

const int kAffordanceOuterRadius = 60;
const int kAffordanceInnerRadius = 50;

// Angles from x-axis at which the outer and inner circles start.
const int kAffordanceOuterStartAngle = -109;
const int kAffordanceInnerStartAngle = -65;

const int kAffordanceGlowWidth = 20;
// The following is half width to avoid division by 2.
const int kAffordanceArcWidth = 3;

// Start and end values for various animations.
const double kAffordanceScaleStartValue = 0.8;
const double kAffordanceScaleEndValue = 1.0;
const double kAffordanceShrinkScaleEndValue = 0.5;
const double kAffordanceOpacityStartValue = 0.1;
const double kAffordanceOpacityEndValue = 0.5;
const int kAffordanceAngleStartValue = 0;
// The end angle is a bit greater than 360 to make sure the circle completes at
// the end of the animation.
const int kAffordanceAngleEndValue = 380;
const int kAffordanceDelayBeforeShrinkMs = 200;
const int kAffordanceShrinkAnimationDurationMs = 100;

// Device bezel operation constants (volume/brightness slider).

// This is the minimal brightness value allowed for the display.
const double kMinBrightnessPercent = 5.0;
// For device operation, the finger is not allowed to enter the screen more
// then this fraction of the size of the screen.
const double kAllowableScreenOverlapForDeviceCommand = 0.0005;

// TODO(skuhne): The noise reduction can be removed when / if we are adding a
// more general reduction.
// To avoid unwanted noise activation, the first 'n' events are being ignored
// for bezel device gestures.
const int kIgnoreFirstBezelDeviceEvents = 10;
// Within these 'n' huge coordinate changes are not allowed. The threshold is
// given in fraction of screen resolution changes.
const int kBezelNoiseDeltaFilter = 0.1;
// To avoid the most frequent noise (extreme locations) the bezel percent
// sliders will not cover the entire screen. We scale therefore the percent
// value by this many percent for minima and maxima extension.
// (Range extends to -kMinMaxInsetPercent .. 100 + kMinMaxInsetPercent).
const double kMinMaxInsetPercent = 5.0;
// To make it possible to reach minimas and maximas easily a range extension
// of -kMinMaxCutOffPercent .. 100 + kMinMaxCutOffPercent will be clamped to
// 0..100%. Everything beyond that will be ignored.
const double kMinMaxCutOffPercent = 2.0;

// Visual constants.
const SkColor kAffordanceGlowStartColor = SkColorSetARGB(24, 255, 255, 255);
const SkColor kAffordanceGlowEndColor = SkColorSetARGB(0, 255, 255, 255);
const SkColor kAffordanceArcColor = SkColorSetARGB(80, 0, 0, 0);
const int kAffordanceFrameRateHz = 60;

const double kPinchThresholdForMaximize = 1.5;
const double kPinchThresholdForMinimize = 0.7;

enum SystemGestureStatus {
  SYSTEM_GESTURE_PROCESSED,  // The system gesture has been processed.
  SYSTEM_GESTURE_IGNORED,    // The system gesture was ignored.
  SYSTEM_GESTURE_END,        // Marks the end of the sytem gesture.
};

aura::Window* GetTargetForSystemGestureEvent(aura::Window* target) {
  aura::Window* system_target = target;
  if (!system_target || system_target == target->GetRootWindow())
    system_target = ash::wm::GetActiveWindow();
  if (system_target)
    system_target = system_target->GetToplevelWindow();
  return system_target;
}

Widget* CreateAffordanceWidget(aura::RootWindow* root_window) {
  Widget* widget = new Widget;
  Widget::InitParams params;
  params.type = Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.keep_on_top = true;
  params.accept_events = false;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.transparent = true;
  widget->Init(params);
  widget->SetOpacity(0xFF);
  widget->GetNativeWindow()->SetParent(
      ash::GetRootWindowController(root_window)->GetContainer(
          ash::internal::kShellWindowId_OverlayContainer));
  return widget;
}

void PaintAffordanceArc(gfx::Canvas* canvas,
                        gfx::Point& center,
                        int radius,
                        int start_angle,
                        int end_angle) {
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(2 * kAffordanceArcWidth);
  paint.setColor(kAffordanceArcColor);
  paint.setAntiAlias(true);

  SkPath arc_path;
  arc_path.addArc(SkRect::MakeXYWH(center.x() - radius,
                                   center.y() - radius,
                                   2 * radius,
                                   2 * radius),
                  start_angle, end_angle);
  canvas->DrawPath(arc_path, paint);
}

void PaintAffordanceGlow(gfx::Canvas* canvas,
                        gfx::Point& center,
                        int start_radius,
                        int end_radius,
                        SkColor* colors,
                        SkScalar* pos,
                        int num_colors) {
  SkPoint sk_center;
  int radius = (end_radius + start_radius) / 2;
  int glow_width = end_radius - start_radius;
  sk_center.iset(center.x(), center.y());
  SkShader* shader = SkGradientShader::CreateTwoPointRadial(
      sk_center,
      SkIntToScalar(start_radius),
      sk_center,
      SkIntToScalar(end_radius),
      colors,
      pos,
      num_colors,
      SkShader::kClamp_TileMode);
  DCHECK(shader);
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(glow_width);
  paint.setShader(shader);
  paint.setAntiAlias(true);
  shader->unref();
  SkPath arc_path;
  arc_path.addArc(SkRect::MakeXYWH(center.x() - radius,
                                   center.y() - radius,
                                   2 * radius,
                                   2 * radius),
                  0, 360);
  canvas->DrawPath(arc_path, paint);
}

}  // namespace

namespace ash {
namespace internal {

// View of the LongPressAffordanceAnimation. Draws the actual contents and
// updates as the animation proceeds. It also maintains the views::Widget that
// the animation is shown in.
class LongPressAffordanceAnimation::LongPressAffordanceView
    : public views::View {
 public:
  LongPressAffordanceView(const gfx::Point& event_location,
                          aura::RootWindow* root_window)
      : views::View(),
        widget_(CreateAffordanceWidget(root_window)),
        current_angle_(kAffordanceAngleStartValue),
        current_scale_(kAffordanceScaleStartValue) {
    widget_->SetContentsView(this);
    widget_->SetAlwaysOnTop(true);

    // We are owned by the LongPressAffordance.
    set_owned_by_client();
    gfx::Point point = event_location;
    aura::client::GetScreenPositionClient(root_window)->ConvertPointToScreen(
        root_window, &point);
    widget_->SetBounds(gfx::Rect(
        point.x() - (kAffordanceOuterRadius + kAffordanceGlowWidth),
        point.y() - (kAffordanceOuterRadius + kAffordanceGlowWidth),
        GetPreferredSize().width(),
        GetPreferredSize().height()));
    widget_->Show();
    widget_->GetNativeView()->layer()->SetOpacity(kAffordanceOpacityStartValue);
  }

  virtual ~LongPressAffordanceView() {
  }

  void UpdateWithGrowAnimation(ui::Animation* animation) {
    // Update the portion of the circle filled so far and re-draw.
    current_angle_ = animation->CurrentValueBetween(kAffordanceAngleStartValue,
        kAffordanceAngleEndValue);
    current_scale_ = animation->CurrentValueBetween(kAffordanceScaleStartValue,
        kAffordanceScaleEndValue);
    widget_->GetNativeView()->layer()->SetOpacity(
        animation->CurrentValueBetween(kAffordanceOpacityStartValue,
            kAffordanceOpacityEndValue));
    SchedulePaint();
  }

  void UpdateWithShrinkAnimation(ui::Animation* animation) {
    current_scale_ = animation->CurrentValueBetween(kAffordanceScaleEndValue,
        kAffordanceShrinkScaleEndValue);
    widget_->GetNativeView()->layer()->SetOpacity(
        animation->CurrentValueBetween(kAffordanceOpacityEndValue,
            kAffordanceOpacityStartValue));
    SchedulePaint();
  }

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(2 * (kAffordanceOuterRadius + kAffordanceGlowWidth),
        2 * (kAffordanceOuterRadius + kAffordanceGlowWidth));
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    gfx::Point center(GetPreferredSize().width() / 2,
                      GetPreferredSize().height() / 2);
    canvas->Save();

    ui::Transform scale;
    scale.SetScale(current_scale_, current_scale_);
    // We want to scale from the center.
    canvas->Translate(gfx::Point(center.x(), center.y()));
    canvas->Transform(scale);
    canvas->Translate(gfx::Point(-center.x(), -center.y()));

    // Paint affordance glow
    int start_radius = kAffordanceInnerRadius - kAffordanceGlowWidth;
    int end_radius = kAffordanceOuterRadius + kAffordanceGlowWidth;
    const int num_colors = 3;
    SkScalar pos[num_colors] = {0, 0.5, 1};
    SkColor colors[num_colors] = {kAffordanceGlowEndColor,
        kAffordanceGlowStartColor, kAffordanceGlowEndColor};
    PaintAffordanceGlow(canvas, center, start_radius, end_radius, colors, pos,
        num_colors);

    // Paint inner circle.
    PaintAffordanceArc(canvas, center, kAffordanceInnerRadius,
        kAffordanceInnerStartAngle, -current_angle_);
    // Paint outer circle.
    PaintAffordanceArc(canvas, center, kAffordanceOuterRadius,
        kAffordanceOuterStartAngle, current_angle_);

    canvas->Restore();
  }

  scoped_ptr<views::Widget> widget_;
  int current_angle_;
  double current_scale_;

  DISALLOW_COPY_AND_ASSIGN(LongPressAffordanceView);
};

LongPressAffordanceAnimation::LongPressAffordanceAnimation()
    : ui::LinearAnimation(kAffordanceFrameRateHz, this),
      view_(NULL),
      tap_down_touch_id_(-1),
      tap_down_display_id_(0),
      current_animation_type_(NONE) {
}

LongPressAffordanceAnimation::~LongPressAffordanceAnimation() {}

void LongPressAffordanceAnimation::ProcessEvent(aura::Window* target,
                                                ui::LocatedEvent* event,
                                                int touch_id) {
  // Once we have a touch id, we are only interested in event of that touch id.
  if (tap_down_touch_id_ != -1 && tap_down_touch_id_ != touch_id)
    return;
  int64 timer_start_time_ms =
      ui::GestureConfiguration::semi_long_press_time_in_seconds() * 1000;
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      // Start animation.
      tap_down_location_ = event->root_location();
      tap_down_touch_id_ = touch_id;
      current_animation_type_ = GROW_ANIMATION;
      tap_down_display_id_ = gfx::Screen::GetDisplayNearestWindow(target).id();
      timer_.Start(FROM_HERE,
                   base::TimeDelta::FromMilliseconds(timer_start_time_ms),
                   this,
                   &LongPressAffordanceAnimation::StartAnimation);
      break;
    case ui::ET_TOUCH_MOVED:
      // If animation is running, We want it to be robust to small finger
      // movements. So we stop the animation only when the finger moves a
      // certain distance.
      if (!ui::gestures::IsInsideManhattanSquare(
          event->root_location(), tap_down_location_))
        StopAnimation();
      break;
    case ui::ET_TOUCH_CANCELLED:
    case ui::ET_GESTURE_END:
      // We will stop the animation on TOUCH_RELEASED.
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      if (is_animating())
        End();
      break;
    default:
      // On all other touch and gesture events, we hide the animation.
      StopAnimation();
      break;
  }
}

void LongPressAffordanceAnimation::StartAnimation() {
  aura::RootWindow* root_window = NULL;
  switch (current_animation_type_) {
    case GROW_ANIMATION:
      root_window = ash::Shell::GetInstance()->display_controller()->
          GetRootWindowForDisplayId(tap_down_display_id_);
      if (!root_window) {
        StopAnimation();
        return;
      }
      view_.reset(new LongPressAffordanceView(tap_down_location_, root_window));
      SetDuration(
          ui::GestureConfiguration::long_press_time_in_seconds() * 1000 -
          ui::GestureConfiguration::semi_long_press_time_in_seconds() * 1000 -
          kAffordanceDelayBeforeShrinkMs);
      Start();
      break;
    case SHRINK_ANIMATION:
      SetDuration(kAffordanceShrinkAnimationDurationMs);
      Start();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void LongPressAffordanceAnimation::StopAnimation() {
  if (timer_.IsRunning())
    timer_.Stop();
  // Since, Animation::Stop() calls AnimationEnded(), we need to reset the
  // |current_animation_type_| before Stop(), otherwise AnimationEnded() may
  // start the timer again.
  current_animation_type_ = NONE;
  if (is_animating())
    Stop();
  view_.reset();
  tap_down_touch_id_ = -1;
  tap_down_display_id_ = 0;
}

void LongPressAffordanceAnimation::AnimateToState(double state) {
  DCHECK(view_.get());
  switch (current_animation_type_) {
    case GROW_ANIMATION:
      view_->UpdateWithGrowAnimation(this);
      break;
    case SHRINK_ANIMATION:
      view_->UpdateWithShrinkAnimation(this);
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool LongPressAffordanceAnimation::ShouldSendCanceledFromStop() {
  return false;
}

void LongPressAffordanceAnimation::AnimationEnded(
    const ui::Animation* animation) {
  switch (current_animation_type_) {
    case GROW_ANIMATION:
      current_animation_type_ = SHRINK_ANIMATION;
      timer_.Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(kAffordanceDelayBeforeShrinkMs),
          this, &LongPressAffordanceAnimation::StartAnimation);
      break;
    case SHRINK_ANIMATION:
      current_animation_type_ = NONE;
      // fall through to reset the view.
    default:
      view_.reset();
      tap_down_touch_id_ = -1;
      tap_down_display_id_ = 0;
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// SystemPinchHandler

class SystemPinchHandler {
 public:
  explicit SystemPinchHandler(aura::Window* target)
      : target_(target),
        phantom_(target),
        phantom_state_(PHANTOM_WINDOW_NORMAL),
        pinch_factor_(1.) {
    widget_ = views::Widget::GetWidgetForNativeWindow(target_);
  }

  ~SystemPinchHandler() {
  }

  SystemGestureStatus ProcessGestureEvent(const ui::GestureEvent& event) {
    // The target has changed, somehow. Let's bale.
    if (!widget_ || !widget_->widget_delegate()->CanResize())
      return SYSTEM_GESTURE_END;

    switch (event.type()) {
      case ui::ET_GESTURE_END: {
        if (event.details().touch_points() > kSystemGesturePoints)
          break;

        if (phantom_state_ == PHANTOM_WINDOW_MAXIMIZED) {
          if (!wm::IsWindowMaximized(target_) &&
              !wm::IsWindowFullscreen(target_))
            wm::MaximizeWindow(target_);
        } else if (phantom_state_ == PHANTOM_WINDOW_MINIMIZED) {
          if (wm::IsWindowMaximized(target_) ||
              wm::IsWindowFullscreen(target_)) {
            wm::RestoreWindow(target_);
          } else {
            wm::MinimizeWindow(target_);

            // NOTE: Minimizing the window will cause this handler to be
            // destroyed. So do not access anything from |this| from here.
            return SYSTEM_GESTURE_END;
          }
        }
        return SYSTEM_GESTURE_END;
      }

      case ui::ET_GESTURE_PINCH_UPDATE: {
        // The PINCH_UPDATE events contain incremental scaling updates.
        pinch_factor_ *= event.details().scale();
        gfx::Rect bounds =
            GetPhantomWindowScreenBounds(target_, event.location());
        if (phantom_state_ != PHANTOM_WINDOW_NORMAL || phantom_.IsShowing())
          phantom_.Show(bounds);
        break;
      }

      case ui::ET_GESTURE_MULTIFINGER_SWIPE: {
        phantom_.Hide();
        pinch_factor_ = 1.0;
        phantom_state_ = PHANTOM_WINDOW_NORMAL;

        if (event.details().swipe_left() || event.details().swipe_right()) {
          // Snap for left/right swipes. In case the window is
          // maximized/fullscreen, then restore the window first so that tiling
          // works correctly.
          if (wm::IsWindowMaximized(target_) ||
              wm::IsWindowFullscreen(target_))
            wm::RestoreWindow(target_);

          ui::ScopedLayerAnimationSettings settings(
              target_->layer()->GetAnimator());
          SnapSizer sizer(target_,
              gfx::Point(),
              event.details().swipe_left() ? internal::SnapSizer::LEFT_EDGE :
                                             internal::SnapSizer::RIGHT_EDGE,
              Shell::GetInstance()->GetGridSize());
          target_->SetBounds(sizer.GetSnapBounds(target_->bounds()));
        } else if (event.details().swipe_up()) {
          if (!wm::IsWindowMaximized(target_) &&
              !wm::IsWindowFullscreen(target_))
            wm::MaximizeWindow(target_);
        } else if (event.details().swipe_down()) {
          wm::MinimizeWindow(target_);
        } else {
          NOTREACHED() << "Swipe happened without a direction.";
        }
        break;
      }

      default:
        break;
    }

    return SYSTEM_GESTURE_PROCESSED;
  }

 private:
  gfx::Rect GetPhantomWindowScreenBounds(aura::Window* window,
                                         const gfx::Point& point) {
    if (pinch_factor_ > kPinchThresholdForMaximize) {
      phantom_state_ = PHANTOM_WINDOW_MAXIMIZED;
      return ScreenAsh::ConvertRectToScreen(
          target_->parent(),
          ScreenAsh::GetMaximizedWindowBoundsInParent(target_));
    }

    if (pinch_factor_ < kPinchThresholdForMinimize) {
      if (wm::IsWindowMaximized(window) || wm::IsWindowFullscreen(window)) {
        const gfx::Rect* restore = GetRestoreBoundsInScreen(window);
        if (restore) {
          phantom_state_ = PHANTOM_WINDOW_MINIMIZED;
          return *restore;
        }
        return window->bounds();
      }

      Launcher* launcher = Shell::GetInstance()->launcher();
      gfx::Rect rect = launcher->GetScreenBoundsOfItemIconForWindow(target_);
      if (rect.IsEmpty())
        rect = launcher->widget()->GetWindowBoundsInScreen();
      else
        rect.Inset(-8, -8);
      phantom_state_ = PHANTOM_WINDOW_MINIMIZED;
      return rect;
    }

    phantom_state_ = PHANTOM_WINDOW_NORMAL;
    return window->bounds();
  }

  enum PhantomWindowState {
    PHANTOM_WINDOW_NORMAL,
    PHANTOM_WINDOW_MAXIMIZED,
    PHANTOM_WINDOW_MINIMIZED,
  };

  aura::Window* target_;
  views::Widget* widget_;

  // A phantom window is used to provide visual cues for
  // pinch-to-resize/maximize/minimize gestures.
  PhantomWindowController phantom_;

  // When the phantom window is in minimized or maximized state, moving the
  // target window should not move the phantom window. So |phantom_state_| is
  // used to track the state of the phantom window.
  PhantomWindowState phantom_state_;

  // PINCH_UPDATE events include incremental pinch-amount. But it is necessary
  // to keep track of the overall pinch-amount. |pinch_factor_| is used for
  // that.
  double pinch_factor_;

  DISALLOW_COPY_AND_ASSIGN(SystemPinchHandler);
};

SystemGestureEventFilter::SystemGestureEventFilter()
    : aura::EventFilter(),
      overlap_percent_(5),
      start_location_(BEZEL_START_UNSET),
      orientation_(SCROLL_ORIENTATION_UNSET),
      is_scrubbing_(false),
      long_press_affordance_(new LongPressAffordanceAnimation) {
}

SystemGestureEventFilter::~SystemGestureEventFilter() {
}

bool SystemGestureEventFilter::PreHandleKeyEvent(aura::Window* target,
                                                 ui::KeyEvent* event) {
  return false;
}

bool SystemGestureEventFilter::PreHandleMouseEvent(aura::Window* target,
                                                   ui::MouseEvent* event) {
#if defined(OS_CHROMEOS)
  if (event->type() == ui::ET_MOUSE_PRESSED && event->native_event() &&
      ui::TouchFactory::GetInstance()->IsTouchDevicePresent()) {
    Shell::GetInstance()->delegate()->RecordUserMetricsAction(
      UMA_MOUSE_DOWN);
  }
#endif
  return false;
}

ui::TouchStatus SystemGestureEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    ui::TouchEvent* event) {
  touch_uma_.RecordTouchEvent(target, *event);
  long_press_affordance_->ProcessEvent(target, event, event->touch_id());
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus SystemGestureEventFilter::PreHandleGestureEvent(
    aura::Window* target, ui::GestureEvent* event) {
  touch_uma_.RecordGestureEvent(target, *event);
  long_press_affordance_->ProcessEvent(target, event,
      event->GetLowestTouchId());
  if (!target || target == target->GetRootWindow()) {
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN:
        HandleBezelGestureStart(target, event);
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE:
        // Check if a valid start position has been set.
        if (start_location_ == BEZEL_START_UNSET)
          break;

        if (DetermineGestureOrientation(event))
          HandleBezelGestureUpdate(target, event);
        break;
      case ui::ET_GESTURE_SCROLL_END:
        HandleBezelGestureEnd();
        break;
      default:
        break;
    }
    return ui::GESTURE_STATUS_CONSUMED;
  }

  aura::Window* system_target = GetTargetForSystemGestureEvent(target);
  if (!system_target)
    return ui::GESTURE_STATUS_UNKNOWN;

  RootWindowController* root_controller =
      GetRootWindowController(system_target->GetRootWindow());
  CHECK(root_controller);
  aura::Window* desktop_container = root_controller->GetContainer(
      ash::internal::kShellWindowId_DesktopBackgroundContainer);
  if (desktop_container->Contains(system_target)) {
    // The gesture was on the desktop window.
    if (event->type() == ui::ET_GESTURE_MULTIFINGER_SWIPE &&
        event->details().swipe_up() &&
        event->details().touch_points() == kSystemGesturePoints) {
      ash::AcceleratorController* accelerator =
          ash::Shell::GetInstance()->accelerator_controller();
      if (accelerator->PerformAction(CYCLE_FORWARD_MRU, ui::Accelerator()))
        return ui::GESTURE_STATUS_CONSUMED;
    }
    return ui::GESTURE_STATUS_UNKNOWN;
  }

  WindowPinchHandlerMap::iterator find = pinch_handlers_.find(system_target);
  if (find != pinch_handlers_.end()) {
    SystemGestureStatus status =
        (*find).second->ProcessGestureEvent(*event);
    if (status == SYSTEM_GESTURE_END)
      ClearGestureHandlerForWindow(system_target);
    return ui::GESTURE_STATUS_CONSUMED;
  } else {
    if (event->type() == ui::ET_GESTURE_BEGIN &&
        event->details().touch_points() >= kSystemGesturePoints) {
      pinch_handlers_[system_target] = new SystemPinchHandler(system_target);
      system_target->AddObserver(this);
      return ui::GESTURE_STATUS_CONSUMED;
    }
  }

  return ui::GESTURE_STATUS_UNKNOWN;
}

void SystemGestureEventFilter::OnWindowVisibilityChanged(aura::Window* window,
                                                         bool visible) {
  if (!visible)
    ClearGestureHandlerForWindow(window);
}

void SystemGestureEventFilter::OnWindowDestroying(aura::Window* window) {
  ClearGestureHandlerForWindow(window);
}

void SystemGestureEventFilter::ClearGestureHandlerForWindow(
    aura::Window* window) {
  WindowPinchHandlerMap::iterator find = pinch_handlers_.find(window);
  if (find == pinch_handlers_.end()) {
    // The handler may have already been removed.
    return;
  }
  delete (*find).second;
  pinch_handlers_.erase(find);
  window->RemoveObserver(this);
}

bool SystemGestureEventFilter::HandleDeviceControl(
    const gfx::Rect& screen,
    ui::GestureEvent* event) {
  // Get the slider position as value from the absolute position.
  // Note that the highest value is at the top.
  double percent = 100.0 - 100.0 * (event->y() - screen.y()) / screen.height();
  if (!DeNoiseBezelSliderPosition(percent)) {
    // Note: Even though this particular event might be noise, the gesture
    // itself is still valid and should not get cancelled.
    return false;
  }
  ash::AcceleratorController* accelerator =
      ash::Shell::GetInstance()->accelerator_controller();
  if (start_location_ == BEZEL_START_LEFT) {
    ash::BrightnessControlDelegate* delegate =
        accelerator->brightness_control_delegate();
    if (delegate)
      delegate->SetBrightnessPercent(
          LimitBezelBrightnessFromSlider(percent), true);
  } else if (start_location_ == BEZEL_START_RIGHT) {
    Shell::GetInstance()->tray_delegate()->GetVolumeControlDelegate()->
        SetVolumePercent(percent);
  } else {
    // No further events are necessary.
    return true;
  }

  // More notifications can be send.
  return false;
}

bool SystemGestureEventFilter::HandleLauncherControl(
    ui::GestureEvent* event) {
  if (start_location_ == BEZEL_START_BOTTOM &&
      event->details().scroll_y() < 0) {
    ash::AcceleratorController* accelerator =
        ash::Shell::GetInstance()->accelerator_controller();
    accelerator->PerformAction(FOCUS_LAUNCHER, ui::Accelerator());
  } else {
    return false;
  }
  // No further notifications for this gesture.
  return true;
}

bool SystemGestureEventFilter::HandleApplicationControl(
    ui::GestureEvent* event) {
  ash::AcceleratorController* accelerator =
      ash::Shell::GetInstance()->accelerator_controller();
  if (start_location_ == BEZEL_START_LEFT && event->details().scroll_x() > 0)
    accelerator->PerformAction(CYCLE_BACKWARD_LINEAR, ui::Accelerator());
  else if (start_location_ == BEZEL_START_RIGHT &&
             event->details().scroll_x() < 0)
    accelerator->PerformAction(CYCLE_FORWARD_LINEAR, ui::Accelerator());
  else
    return false;

  // No further notifications for this gesture.
  return true;
}

void SystemGestureEventFilter::HandleBezelGestureStart(
    aura::Window* target, ui::GestureEvent* event) {
  gfx::Rect screen = gfx::Screen::GetDisplayNearestWindow(target).bounds();
  int overlap_area = screen.width() * overlap_percent_ / 100;
  orientation_ = SCROLL_ORIENTATION_UNSET;

  if (event->x() <= screen.x() + overlap_area) {
    start_location_ = BEZEL_START_LEFT;
  } else if (event->x() >= screen.right() - overlap_area) {
    start_location_ = BEZEL_START_RIGHT;
  } else if (event->y() >= screen.bottom()) {
    start_location_ = BEZEL_START_BOTTOM;
  }
}

bool SystemGestureEventFilter::DetermineGestureOrientation(
    ui::GestureEvent* event) {
  if (orientation_ == SCROLL_ORIENTATION_UNSET) {
    if (!event->details().scroll_x() && !event->details().scroll_y())
      return false;

    // For left and right the scroll angle needs to be much steeper to
    // be accepted for a 'device configuration' gesture.
    if (start_location_ == BEZEL_START_LEFT ||
        start_location_ == BEZEL_START_RIGHT) {
      orientation_ = abs(event->details().scroll_y()) >
                     abs(event->details().scroll_x()) * 3 ?
          SCROLL_ORIENTATION_VERTICAL : SCROLL_ORIENTATION_HORIZONTAL;
    } else {
      orientation_ = abs(event->details().scroll_y()) >
                     abs(event->details().scroll_x()) ?
          SCROLL_ORIENTATION_VERTICAL : SCROLL_ORIENTATION_HORIZONTAL;
    }

    // Reset the delay counter for noise event filtering.
    initiation_delay_events_ = 0;
  }
  return true;
}

void SystemGestureEventFilter::HandleBezelGestureUpdate(
    aura::Window* target, ui::GestureEvent* event) {
  if (orientation_ == SCROLL_ORIENTATION_HORIZONTAL) {
    if (HandleApplicationControl(event))
      start_location_ = BEZEL_START_UNSET;
  } else {
    if (start_location_ == BEZEL_START_BOTTOM) {
      if (HandleLauncherControl(event))
        start_location_ = BEZEL_START_UNSET;
    } else {
      // Check if device gestures should be performed or not.
      if (CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableBezelTouch)) {
        start_location_ = BEZEL_START_UNSET;
        return;
      }
      gfx::Rect screen = gfx::Screen::GetDisplayNearestWindow(target).bounds();
      // Limit the user gesture "mostly" to the off screen area and check for
      // noise invocation.
      if (!GestureInBezelArea(screen, event) ||
          BezelGestureMightBeNoise(screen, event))
        return;
      if (HandleDeviceControl(screen, event))
        start_location_ = BEZEL_START_UNSET;
    }
  }
}

void SystemGestureEventFilter::HandleBezelGestureEnd() {
  // All which is needed is to set the gesture start location to undefined.
  start_location_ = BEZEL_START_UNSET;
}

bool SystemGestureEventFilter::GestureInBezelArea(
    const gfx::Rect& screen, ui::GestureEvent* event) {
  // Limit the gesture mostly to the off screen.
  double allowable_offset =
      screen.width() * kAllowableScreenOverlapForDeviceCommand;
  if ((start_location_ == BEZEL_START_LEFT &&
       event->x() > allowable_offset) ||
      (start_location_ == BEZEL_START_RIGHT &&
       event->x() < screen.width() - allowable_offset)) {
    start_location_ = BEZEL_START_UNSET;
    return false;
  }
  return true;
}

bool SystemGestureEventFilter::BezelGestureMightBeNoise(
    const gfx::Rect& screen, ui::GestureEvent* event) {
  // The first events will not trigger an action.
  if (initiation_delay_events_++ < kIgnoreFirstBezelDeviceEvents) {
    // When the values are too far apart we ignore it since it might
    // be random noise.
    double delta_y = event->details().scroll_y();
    double span_y = screen.height();
    if (abs(delta_y / span_y) > kBezelNoiseDeltaFilter)
      start_location_ = BEZEL_START_UNSET;
    return true;
  }
  return false;
}

bool SystemGestureEventFilter::DeNoiseBezelSliderPosition(double& percent) {
  // The range gets passed as 0..100% and is extended to the range of
  // (-kMinMaxInsetPercent) .. (100 + kMinMaxInsetPercent). This way we can
  // cut off the extreme upper and lower values which are prone to noise.
  // It additionally adds a "security buffer" which can then be clamped to the
  // extremes to empower the user to get to these values (0% and 100%).
  percent = percent * (100.0 + 2 * kMinMaxInsetPercent) / 100 -
      kMinMaxInsetPercent;
  // Values which fall outside of the acceptable inner range area get ignored.
  if (percent < -kMinMaxCutOffPercent ||
      percent > 100.0 + kMinMaxCutOffPercent)
    return false;
  // Excessive values get then clamped to the 0..100% range.
  percent = std::max(std::min(percent, 100.0), 0.0);
  return true;
}

double SystemGestureEventFilter::LimitBezelBrightnessFromSlider(
    double percent) {
  // Turning off the display makes no sense, so we map the accessible range to
  // kMinimumBrightness .. 100%.
  percent = (percent + kMinBrightnessPercent) * 100.0 /
      (100.0 + kMinBrightnessPercent);
  // Clamp to avoid rounding issues.
  return std::min(percent, 100.0);
}

}  // namespace internal
}  // namespace ash
