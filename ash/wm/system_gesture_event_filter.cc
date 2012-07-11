// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_gesture_event_filter.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/brightness/brightness_control_delegate.h"
#include "ash/volume_control_delegate.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/base/gestures/gesture_configuration.h"
#include "ui/base/gestures/gesture_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {
using views::Widget;

const int kSystemPinchPoints = 4;

const int kAffordanceRadius = 40;
const int kAffordanceWidth = 3;
const SkColor kAffordanceFullColor = SkColorSetARGB(125, 0, 0, 255);
const SkColor kAffordanceEmptyColor = SkColorSetARGB(50, 0, 0, 255);
const int kAffordanceFrameRateHz = 60;

const double kPinchThresholdForMaximize = 1.5;
const double kPinchThresholdForMinimize = 0.7;
const double kPinchThresholdForResize = 0.1;

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

Widget* CreateAffordanceWidget() {
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
      ash::Shell::GetPrimaryRootWindowController()->GetContainer(
          ash::internal::kShellWindowId_OverlayContainer));
  ash::SetWindowVisibilityAnimationTransition(widget->GetNativeView(),
      ash::ANIMATE_HIDE);
  return widget;
}

}  // namespace

namespace ash {
namespace internal {

// View of the LongPressAffordanceAnimation. Draws the actual contents and
// updates as the animation proceeds. It also maintains the views::Widget that
// the animation is shown in.
// Currently the affordance is to simply show an empty circle and fill it up as
// the animation proceeds.
// TODO(varunjain): Change the look of this affordance when we get official UX.
class LongPressAffordanceAnimation::LongPressAffordanceView
    : public views::View {
 public:
  explicit LongPressAffordanceView(const gfx::Point& event_location)
      : views::View(),
        widget_(CreateAffordanceWidget()),
        current_angle_(0) {
    widget_->SetContentsView(this);
    widget_->SetAlwaysOnTop(true);

    // We are owned by the LongPressAffordance.
    set_owned_by_client();
    widget_->SetBounds(gfx::Rect(event_location.x() - kAffordanceRadius,
                                 event_location.y() - kAffordanceRadius,
                                 GetPreferredSize().width(),
                                 GetPreferredSize().height()));
    widget_->Show();
  }

  virtual ~LongPressAffordanceView() {
  }

  void UpdateWithAnimation(ui::Animation* animation) {
    // Update the portion of the circle filled so far and re-draw.
    current_angle_ = animation->CurrentValueBetween(0, 360);
    SchedulePaint();
  }

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(2 * kAffordanceRadius, 2 * kAffordanceRadius);
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(kAffordanceWidth);
    paint.setColor(kAffordanceEmptyColor);

    // Draw empty circle.
    canvas->DrawCircle(gfx::Point(kAffordanceRadius, kAffordanceRadius),
        kAffordanceRadius - kAffordanceWidth, paint);

    // Then draw the portion filled so far by the animation.
    SkPath path;
    path.addArc(
        SkRect::MakeXYWH(kAffordanceWidth, kAffordanceWidth,
                         2 * (kAffordanceRadius - kAffordanceWidth),
                         2 * (kAffordanceRadius - kAffordanceWidth)),
        -90, current_angle_);
    paint.setColor(kAffordanceFullColor);
    canvas->DrawPath(path, paint);
  }

  scoped_ptr<views::Widget> widget_;
  int current_angle_;

  DISALLOW_COPY_AND_ASSIGN(LongPressAffordanceView);
};

LongPressAffordanceAnimation::LongPressAffordanceAnimation()
    : ui::LinearAnimation(kAffordanceFrameRateHz, this),
      view_(NULL),
      tap_down_target_(NULL) {
  int duration =
      ui::GestureConfiguration::long_press_time_in_seconds() * 1000 -
      ui::GestureConfiguration::semi_long_press_time_in_seconds() * 1000;
  SetDuration(duration);
}

LongPressAffordanceAnimation::~LongPressAffordanceAnimation() {}

void LongPressAffordanceAnimation::ProcessEvent(aura::Window* target,
                                                aura::LocatedEvent* event) {
  // Once we have a target, we are only interested in events on that target.
  if (tap_down_target_ && tap_down_target_ != target)
    return;
  int64 timer_start_time_ms =
      ui::GestureConfiguration::semi_long_press_time_in_seconds() * 1000;
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      // Start animation.
      tap_down_location_ = event->root_location();
      tap_down_target_ = target;
      timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(timer_start_time_ms),
                    this,
                    &LongPressAffordanceAnimation::StartAnimation);
      break;
    case ui::ET_TOUCH_MOVED:
      // If animation is running, We want it to be robust to small finger
      // movements. So we stop the animation only when the finger moves a
      // certain distance.
      if (is_animating() && !ui::gestures::IsInsideManhattanSquare(
          event->root_location(), tap_down_location_))
        StopAnimation();
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      if (is_animating())
        End();
      // fall through to default to reset the view and tap down target.
    default:
      // On all other touch and gesture events, we hide the animation.
      StopAnimation();
      break;
  }
}

void LongPressAffordanceAnimation::StartAnimation() {
  view_.reset(new LongPressAffordanceView(tap_down_location_));
  Start();
}

void LongPressAffordanceAnimation::StopAnimation() {
  if (timer_.IsRunning())
    timer_.Stop();
  if (is_animating())
    Stop();
  view_.reset();
  tap_down_target_ = NULL;
}

void LongPressAffordanceAnimation::AnimateToState(double state) {
  DCHECK(view_.get());
  view_->UpdateWithAnimation(this);
}

void LongPressAffordanceAnimation::AnimationEnded(
    const ui::Animation* animation) {
  view_.reset();
  tap_down_target_ = NULL;
}

void LongPressAffordanceAnimation::AnimationProgressed(
    const ui::Animation* animation) {
}

void LongPressAffordanceAnimation::AnimationCanceled(
    const ui::Animation* animation) {
  view_.reset();
  tap_down_target_ = NULL;
}

class SystemPinchHandler {
 public:
  explicit SystemPinchHandler(aura::Window* target)
      : target_(target),
        phantom_(target),
        phantom_state_(PHANTOM_WINDOW_NORMAL),
        pinch_factor_(1.),
        resize_started_(false) {
    widget_ = views::Widget::GetWidgetForNativeWindow(target_);
  }

  ~SystemPinchHandler() {
  }

  SystemGestureStatus ProcessGestureEvent(const aura::GestureEvent& event) {
    // The target has changed, somehow. Let's bale.
    if (!widget_ || !widget_->widget_delegate()->CanResize())
      return SYSTEM_GESTURE_END;

    switch (event.type()) {
      case ui::ET_GESTURE_END: {
        if (event.details().touch_points() > kSystemPinchPoints)
          break;

        if (resize_started_) {
          if (phantom_state_ == PHANTOM_WINDOW_MAXIMIZED) {
            wm::MaximizeWindow(target_);
          } else if (phantom_state_ == PHANTOM_WINDOW_MINIMIZED) {
            wm::MinimizeWindow(target_);
            // NOTE: Minimizing the window will cause this handler to be
            // destroyed. So do not access anything from |this| from here.
            return SYSTEM_GESTURE_END;
          } else {
            gfx::Rect bounds = phantom_.IsShowing() ?  phantom_.bounds() :
              target_->bounds();
            int grid = Shell::GetInstance()->GetGridSize();
            bounds.set_x(WindowResizer::AlignToGridRoundUp(bounds.x(), grid));
            bounds.set_y(WindowResizer::AlignToGridRoundUp(bounds.y(), grid));
            if (wm::IsWindowFullscreen(target_) ||
                wm::IsWindowMaximized(target_)) {
              SetRestoreBounds(target_, bounds);
              wm::RestoreWindow(target_);
            } else {
              target_->SetBounds(bounds);
            }
          }
        }
        return SYSTEM_GESTURE_END;
      }

      case ui::ET_GESTURE_SCROLL_UPDATE: {
        if (wm::IsWindowFullscreen(target_) || wm::IsWindowMaximized(target_)) {
          if (!phantom_.IsShowing())
            break;
        } else {
          gfx::Rect bounds = target_->bounds();
          bounds.set_x(
              static_cast<int>(bounds.x() + event.details().scroll_x()));
          bounds.set_y(
              static_cast<int>(bounds.y() + event.details().scroll_y()));
          target_->SetBounds(bounds);
        }

        if (phantom_.IsShowing() && phantom_state_ == PHANTOM_WINDOW_NORMAL) {
          gfx::Rect bounds = phantom_.bounds();
          bounds.set_x(
              static_cast<int>(bounds.x() + event.details().scroll_x()));
          bounds.set_y(
              static_cast<int>(bounds.y() + event.details().scroll_y()));
          phantom_.SetBounds(bounds);
        }
        break;
      }

      case ui::ET_GESTURE_PINCH_UPDATE: {
        // The PINCH_UPDATE events contain incremental scaling updates.
        pinch_factor_ *= event.details().scale();
        if (!resize_started_) {
          if (fabs(pinch_factor_ - 1.) < kPinchThresholdForResize)
            break;
          resize_started_ = true;
        }

        gfx::Rect bounds = target_->bounds();

        if (wm::IsWindowFullscreen(target_) || wm::IsWindowMaximized(target_)) {
          // For a fullscreen/maximized window, if you start pinching in, and
          // you pinch enough, then it shows the phantom window with the
          // restore-bounds. The subsequent pinch updates then work on the
          // restore bounds instead of the fullscreen/maximized bounds.
          const gfx::Rect* restore = NULL;
          if (phantom_.IsShowing()) {
            restore = GetRestoreBounds(target_);
          } else if (pinch_factor_ < 0.8) {
            restore = GetRestoreBounds(target_);
            // Reset the pinch factor.
            pinch_factor_ = 1.0;
          }

          if (restore)
            bounds = *restore;
          else
            break;
        }

        phantom_.Show(GetPhantomWindowBounds(bounds, event.location()));
        break;
      }

      case ui::ET_GESTURE_MULTIFINGER_SWIPE: {
        // Snap for left/right swipes.
        if (event.details().swipe_left() || event.details().swipe_right()) {
          ui::ScopedLayerAnimationSettings settings(
              target_->layer()->GetAnimator());
          SnapSizer sizer(target_,
              gfx::Point(),
              event.details().swipe_left() ? internal::SnapSizer::LEFT_EDGE :
                                             internal::SnapSizer::RIGHT_EDGE,
              Shell::GetInstance()->GetGridSize());
          target_->SetBounds(sizer.GetSnapBounds(target_->bounds()));
          phantom_.Hide();
          pinch_factor_ = 1.0;
        }
        break;
      }

      default:
        break;
    }

    return SYSTEM_GESTURE_PROCESSED;
  }

 private:
  gfx::Rect GetPhantomWindowBounds(const gfx::Rect& bounds,
                                   const gfx::Point& point) {
    if (pinch_factor_ > kPinchThresholdForMaximize) {
      phantom_state_ = PHANTOM_WINDOW_MAXIMIZED;
      return ScreenAsh::GetMaximizedWindowBounds(target_);
    }

    if (pinch_factor_ < kPinchThresholdForMinimize) {
      Launcher* launcher = Shell::GetInstance()->launcher();
      gfx::Rect rect = launcher->GetScreenBoundsOfItemIconForWindow(target_);
      if (rect.IsEmpty())
        rect = launcher->widget()->GetWindowScreenBounds();
      else
        rect.Inset(-8, -8);
      phantom_state_ = PHANTOM_WINDOW_MINIMIZED;
      return rect;
    }

    gfx::Rect new_bounds = bounds.Scale(pinch_factor_);
    new_bounds.set_x(bounds.x() + (point.x() - point.x() * pinch_factor_));
    new_bounds.set_y(bounds.y() + (point.y() - point.y() * pinch_factor_));

    gfx::Rect maximize_bounds = ScreenAsh::GetMaximizedWindowBounds(target_);
    if (new_bounds.width() > maximize_bounds.width() ||
        new_bounds.height() > maximize_bounds.height()) {
      phantom_state_ = PHANTOM_WINDOW_MAXIMIZED;
      return maximize_bounds;
    }

    phantom_state_ = PHANTOM_WINDOW_NORMAL;
    return new_bounds;
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

  // Pinch-to-resize starts only after the pinch crosses a certain threshold to
  // make it easier to move window without accidentally resizing the window at
  // the same time. |resize_started_| keeps track of whether pinch crossed the
  // threshold to initiate a window-resize.
  bool resize_started_;

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
                                                 aura::KeyEvent* event) {
  return false;
}

bool SystemGestureEventFilter::PreHandleMouseEvent(aura::Window* target,
                                                   aura::MouseEvent* event) {
  return false;
}

ui::TouchStatus SystemGestureEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  touch_uma_.RecordTouchEvent(target, *event);
  long_press_affordance_->ProcessEvent(target, event);
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus SystemGestureEventFilter::PreHandleGestureEvent(
    aura::Window* target, aura::GestureEvent* event) {
  touch_uma_.RecordGestureEvent(target, *event);
  long_press_affordance_->ProcessEvent(target, event);
  if (!target || target == target->GetRootWindow()) {
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN: {
          gfx::Rect screen =
              gfx::Screen::GetDisplayNearestWindow(target).bounds();
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
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE:
        if (start_location_ == BEZEL_START_UNSET)
          break;
        if (orientation_ == SCROLL_ORIENTATION_UNSET) {
          if (!event->details().scroll_x() && !event->details().scroll_y())
            break;
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
        }
        if (orientation_ == SCROLL_ORIENTATION_HORIZONTAL) {
          if (HandleApplicationControl(event))
            start_location_ = BEZEL_START_UNSET;
        } else {
          if (start_location_ == BEZEL_START_BOTTOM) {
            if (HandleLauncherControl(event))
              start_location_ = BEZEL_START_UNSET;
          } else {
            if (HandleDeviceControl(target, event))
              start_location_ = BEZEL_START_UNSET;
          }
        }
        break;
      case ui::ET_GESTURE_SCROLL_END:
        start_location_ = BEZEL_START_UNSET;
        break;
      default:
        break;
    }
    return ui::GESTURE_STATUS_CONSUMED;
  }

  aura::Window* system_target = GetTargetForSystemGestureEvent(target);
  if (!system_target)
    return ui::GESTURE_STATUS_UNKNOWN;

  WindowPinchHandlerMap::iterator find = pinch_handlers_.find(system_target);
  if (find != pinch_handlers_.end()) {
    SystemGestureStatus status =
        (*find).second->ProcessGestureEvent(*event);
    if (status == SYSTEM_GESTURE_END)
      ClearGestureHandlerForWindow(system_target);
    return ui::GESTURE_STATUS_CONSUMED;
  } else {
    if (event->type() == ui::ET_GESTURE_BEGIN &&
        event->details().touch_points() >= kSystemPinchPoints) {
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

bool SystemGestureEventFilter::HandleDeviceControl(aura::Window* target,
                                                   aura::GestureEvent* event) {
  gfx::Rect screen = gfx::Screen::GetDisplayNearestWindow(target).bounds();
  double percent = 100.0 * (event->y() - screen.y()) / screen.height();
  if (percent > 100.0)
    percent = 100.0;
  if (percent < 0.0)
    percent = 0.0;
  ash::AcceleratorController* accelerator =
      ash::Shell::GetInstance()->accelerator_controller();
  if (start_location_ == BEZEL_START_LEFT) {
    ash::BrightnessControlDelegate* delegate =
        accelerator->brightness_control_delegate();
    if (delegate)
      delegate->SetBrightnessPercent(100.0 - percent, true);
  } else if (start_location_ == BEZEL_START_RIGHT) {
    ash::VolumeControlDelegate* delegate =
        accelerator->volume_control_delegate();
    if (delegate)
      delegate->SetVolumePercent(100.0 - percent);
  } else {
    return true;
  }
  // More notifications can be send.
  return false;
}

bool SystemGestureEventFilter::HandleLauncherControl(
    aura::GestureEvent* event) {
  ash::AcceleratorController* accelerator =
      ash::Shell::GetInstance()->accelerator_controller();
  if (start_location_ == BEZEL_START_BOTTOM && event->details().scroll_y() < 0)
    // We leave the work to switch to the next window to our accelerators.
    accelerator->AcceleratorPressed(
        ui::Accelerator(ui::VKEY_LWIN, ui::EF_CONTROL_DOWN));
  else
    return false;
  // No further notifications for this gesture.
  return true;
}

bool SystemGestureEventFilter::HandleApplicationControl(
    aura::GestureEvent* event) {
  ash::AcceleratorController* accelerator =
      ash::Shell::GetInstance()->accelerator_controller();
  if (start_location_ == BEZEL_START_LEFT && event->details().scroll_x() > 0) {
    // We leave the work to switch to the next window to our accelerators.
    accelerator->AcceleratorPressed(
        ui::Accelerator(ui::VKEY_F5, ui::EF_SHIFT_DOWN));
  } else if (start_location_ == BEZEL_START_RIGHT &&
             event->details().scroll_x() < 0) {
    // We leave the work to switch to the previous window to our accelerators.
    accelerator->AcceleratorPressed(
        ui::Accelerator(ui::VKEY_F5, ui::EF_NONE));
  } else {
    return false;
  }
  // No further notifications for this gesture.
  return true;
}

}  // namespace internal
}  // namespace ash
