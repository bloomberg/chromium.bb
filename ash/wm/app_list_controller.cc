// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/app_list_controller.h"

#include "ash/ash_switches.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/app_list/app_list_view.h"
#include "ui/app_list/icon_cache.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform_util.h"

namespace ash {
namespace internal {

namespace {

const float kContainerAnimationScaleFactor = 1.05f;

// Duration for both default container and app list animation in milliseconds.
const int kAnimationDurationMs = 130;

// Delayed time of 2nd animation in milliseconds.
const int kSecondAnimationStartDelay = kAnimationDurationMs - 20;

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

// Bounds returned is used for full screen app list. Use full monitor rect
// so that the app list shade goes behind the launcher.
gfx::Rect GetFullScreenBoundsForWidget(views::Widget* widget) {
  gfx::NativeView window = widget->GetNativeView();
  return gfx::Screen::GetMonitorNearestWindow(window).bounds();
}

// Return work area rect for full screen app list layout. This function is
// needed to get final work area in one shot instead of waiting for shelf
// animation to finish.
gfx::Rect GetWorkAreaBoundsForWidget(views::Widget* widget) {
  gfx::NativeView window = widget->GetNativeView();
  return Shell::GetInstance()->shelf()->IsVisible() ?
      ScreenAsh::GetUnmaximizedWorkAreaBounds(window) :
      gfx::Screen::GetMonitorNearestWindow(window).work_area();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListController, public:

AppListController::AppListController() : is_visible_(false), view_(NULL) {
  app_list::IconCache::CreateInstance();
}

AppListController::~AppListController() {
  ResetView();
  app_list::IconCache::DeleteInstance();
}

// static
bool AppListController::UseAppListV2() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAppListV2);
}

void AppListController::SetVisible(bool visible) {
  if (visible == is_visible_)
    return;

  is_visible_ = visible;

  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  Shell::GetInstance()->shelf()->UpdateAutoHideState();

  if (view_) {
    ScheduleAnimation();
  } else if (is_visible_) {
    // AppListModel and AppListViewDelegate are owned by AppListView. They
    // will be released with AppListView on close.
    app_list::AppListView* view = new app_list::AppListView(
        Shell::GetInstance()->delegate()->CreateAppListViewDelegate());
    if (UseAppListV2()) {
      view->InitAsBubble(
          Shell::GetInstance()->GetContainer(kShellWindowId_AppListContainer),
          Shell::GetInstance()->launcher()->GetAppListButtonView());
    } else {
      views::Widget* launcher_widget =
          Shell::GetInstance()->launcher()->widget();
      view->InitAsFullscreenWidget(Shell::GetInstance()->GetContainer(
          kShellWindowId_AppListContainer),
          GetFullScreenBoundsForWidget(launcher_widget),
          GetWorkAreaBoundsForWidget(launcher_widget));
    }
    SetView(view);
  }
}

bool AppListController::IsVisible() const {
  return view_ && view_->GetWidget()->IsVisible();
}

aura::Window* AppListController::GetWindow() {
  return is_visible_ && view_ ? view_->GetWidget()->GetNativeWindow() : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, private:

void AppListController::SetView(app_list::AppListView* view) {
  DCHECK(view_ == NULL);

  if (is_visible_) {
    app_list::IconCache::GetInstance()->MarkAllEntryUnused();

    view_ = view;
    views::Widget* widget = view_->GetWidget();
    widget->AddObserver(this);
    Shell::GetInstance()->AddRootWindowEventFilter(this);
    widget->GetNativeView()->GetRootWindow()->AddRootWindowObserver(this);

    widget->SetOpacity(0);
    ScheduleAnimation();

    view_->GetWidget()->Show();
  } else {
    view->GetWidget()->Close();
  }
}

void AppListController::ResetView() {
  if (!view_)
    return;

  views::Widget* widget = view_->GetWidget();
  widget->RemoveObserver(this);
  GetLayer(widget)->GetAnimator()->RemoveObserver(this);
  Shell::GetInstance()->RemoveRootWindowEventFilter(this);
  widget->GetNativeView()->GetRootWindow()->RemoveRootWindowObserver(this);
  view_ = NULL;

  app_list::IconCache::GetInstance()->PurgeAllUnused();
}

void AppListController::ScheduleAnimation() {
  second_animation_timer_.Stop();

  // Stop observing previous animation.
  StopObservingImplicitAnimations();

  ScheduleDimmingAnimation();

  // No browser window fading and app list secondary animation for v2.
  if (UseAppListV2())
    return;

  if (is_visible_) {
    ScheduleBrowserWindowsAnimation();
    second_animation_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kSecondAnimationStartDelay),
        this,
        &AppListController::ScheduleAppListAnimation);
  } else {
    ScheduleAppListAnimation();
    second_animation_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kSecondAnimationStartDelay),
        this,
        &AppListController::ScheduleBrowserWindowsAnimation);
  }
}

void AppListController::ScheduleBrowserWindowsAnimationForContainer(
    aura::Window* container) {
  DCHECK(container);
  ui::Layer* layer = container->layer();
  layer->GetAnimator()->StopAnimating();

  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
  animation.SetTweenType(
      is_visible_ ? ui::Tween::EASE_IN : ui::Tween::EASE_OUT);

  layer->SetOpacity(is_visible_ ? 0.0 : 1.0);
  layer->SetTransform(is_visible_ ?
      ui::GetScaleTransform(
          gfx::Point(layer->bounds().width() / 2,
                     layer->bounds().height() / 2),
          kContainerAnimationScaleFactor) :
      ui::Transform());
}

void AppListController::ScheduleBrowserWindowsAnimation() {
  // Note: containers could be NULL during Shell shutdown.
  aura::Window* default_container = Shell::GetInstance()->GetContainer(
      kShellWindowId_DefaultContainer);
  if (default_container)
    ScheduleBrowserWindowsAnimationForContainer(default_container);
  aura::Window* always_on_top_container = Shell::GetInstance()->
      GetContainer(kShellWindowId_AlwaysOnTopContainer);
  if (always_on_top_container)
    ScheduleBrowserWindowsAnimationForContainer(always_on_top_container);
}

void AppListController::ScheduleDimmingAnimation() {
  ui::Layer* layer = GetLayer(view_->GetWidget());
  layer->GetAnimator()->StopAnimating();

  int duration = UseAppListV2() ?
      kAnimationDurationMs : 2 * kAnimationDurationMs;

  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(duration));
  animation.AddObserver(this);

  layer->SetOpacity(is_visible_ ? 1.0 : 0.0);
}

void AppListController::ScheduleAppListAnimation() {
  if (is_visible_)
    view_->AnimateShow(kAnimationDurationMs);
  else
    view_->AnimateHide(kAnimationDurationMs);
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, aura::EventFilter implementation:

bool AppListController::PreHandleKeyEvent(aura::Window* target,
                                          aura::KeyEvent* event) {
  return false;
}

bool AppListController::PreHandleMouseEvent(aura::Window* target,
                                            aura::MouseEvent* event) {
  if (view_ && is_visible_ && event->type() == ui::ET_MOUSE_PRESSED) {
    views::Widget* widget = view_->GetWidget();
    aura::MouseEvent translated(*event, target, widget->GetNativeView());
    if (!widget->GetNativeView()->ContainsPoint(translated.location()))
      SetVisible(false);
  }
  return false;
}

ui::TouchStatus AppListController::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus AppListController::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
// AppListController,  aura::RootWindowObserver implementation:
void AppListController::OnRootWindowResized(const aura::RootWindow* root,
                                            const gfx::Size& old_size) {
  if (view_ && is_visible_) {
    views::Widget* launcher_widget =
        Shell::GetInstance()->launcher()->widget();
    view_->UpdateBounds(GetFullScreenBoundsForWidget(launcher_widget),
                        GetWorkAreaBoundsForWidget(launcher_widget));
  }
}

void AppListController::OnWindowFocused(aura::Window* window) {
  if (view_ && is_visible_) {
    aura::Window* applist_container = Shell::GetInstance()->GetContainer(
        kShellWindowId_AppListContainer);
    aura::Window* bubble_container = Shell::GetInstance()->GetContainer(
        kShellWindowId_SettingBubbleContainer);
    if (window->parent() != applist_container &&
        window->parent() != bubble_container) {
      SetVisible(false);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, ui::ImplicitAnimationObserver implementation:

void AppListController::OnImplicitAnimationsCompleted() {
  if (is_visible_ )
    view_->GetWidget()->Activate();
  else
    view_->GetWidget()->Close();
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, views::Widget::Observer implementation:

void AppListController::OnWidgetClosing(views::Widget* widget) {
  DCHECK(view_->GetWidget() == widget);
  if (is_visible_)
    SetVisible(false);
  ResetView();
}

}  // namespace internal
}  // namespace ash
