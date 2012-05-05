// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list.h"

#include "ash/app_list/app_list_view.h"
#include "ash/app_list/icon_cache.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_util.h"
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

// Gets preferred bounds of app list window.
gfx::Rect GetPreferredBounds() {
  gfx::Point cursor = gfx::Screen::GetCursorScreenPoint();
  // Use full monitor rect so that the app list shade goes behind the launcher.
  return gfx::Screen::GetMonitorNearestPoint(cursor).bounds();
}

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppList, public:

AppList::AppList() : is_visible_(false), view_(NULL) {
  IconCache::CreateInstance();
}

AppList::~AppList() {
  ResetView();
  IconCache::DeleteInstance();
}

void AppList::SetVisible(bool visible) {
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
    SetView(new AppListView(
        Shell::GetInstance()->delegate()->CreateAppListViewDelegate(),
        GetPreferredBounds()));
  }
}

bool AppList::IsVisible() {
  return view_ && view_->GetWidget()->IsVisible();
}

aura::Window* AppList::GetWindow() {
  return is_visible_ && view_ ? view_->GetWidget()->GetNativeWindow() : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// AppList, private:

void AppList::SetView(AppListView* view) {
  DCHECK(view_ == NULL);

  if (is_visible_) {
    IconCache::GetInstance()->MarkAllEntryUnused();

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

void AppList::ResetView() {
  if (!view_)
    return;

  views::Widget* widget = view_->GetWidget();
  widget->RemoveObserver(this);
  GetLayer(widget)->GetAnimator()->RemoveObserver(this);
  Shell::GetInstance()->RemoveRootWindowEventFilter(this);
  widget->GetNativeView()->GetRootWindow()->RemoveRootWindowObserver(this);
  view_ = NULL;

  IconCache::GetInstance()->PurgeAllUnused();
}

void AppList::ScheduleAnimation() {
  second_animation_timer_.Stop();

  // Stop observing previous animation.
  StopObservingImplicitAnimations();

  ScheduleDimmingAnimation();

  if (is_visible_) {
    ScheduleBrowserWindowsAnimation();
    second_animation_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kSecondAnimationStartDelay),
        this,
        &AppList::ScheduleAppListAnimation);
  } else {
    ScheduleAppListAnimation();
    second_animation_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kSecondAnimationStartDelay),
        this,
        &AppList::ScheduleBrowserWindowsAnimation);
  }

}

void AppList::ScheduleBrowserWindowsAnimationForContainer(
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

void AppList::ScheduleBrowserWindowsAnimation() {
  // Note: containers could be NULL during Shell shutdown.
  aura::Window* default_container = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_DefaultContainer);
  if (default_container)
    ScheduleBrowserWindowsAnimationForContainer(default_container);
  aura::Window* always_on_top_container = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_AlwaysOnTopContainer);
  if (always_on_top_container)
    ScheduleBrowserWindowsAnimationForContainer(always_on_top_container);
}

void AppList::ScheduleDimmingAnimation() {
  ui::Layer* layer = GetLayer(view_->GetWidget());
  layer->GetAnimator()->StopAnimating();

  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(2 * kAnimationDurationMs));
  animation.AddObserver(this);

  layer->SetOpacity(is_visible_ ? 1.0 : 0.0);
}

void AppList::ScheduleAppListAnimation() {
  if (is_visible_)
    view_->AnimateShow(kAnimationDurationMs);
  else
    view_->AnimateHide(kAnimationDurationMs);
}

////////////////////////////////////////////////////////////////////////////////
// AppList, aura::EventFilter implementation:

bool AppList::PreHandleKeyEvent(aura::Window* target,
                                aura::KeyEvent* event) {
  return false;
}

bool AppList::PreHandleMouseEvent(aura::Window* target,
                                  aura::MouseEvent* event) {
  if (view_ && is_visible_ && event->type() == ui::ET_MOUSE_PRESSED) {
    views::Widget* widget = view_->GetWidget();
    aura::MouseEvent translated(*event, target, widget->GetNativeView());
    if (!widget->GetNativeView()->ContainsPoint(translated.location()))
      SetVisible(false);
  }
  return false;
}

ui::TouchStatus AppList::PreHandleTouchEvent(aura::Window* target,
                                             aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus AppList::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
// AppList,  ura::RootWindowObserver implementation:
void AppList::OnRootWindowResized(const aura::RootWindow* root,
                                  const gfx::Size& old_size) {
  if (view_&& is_visible_)
    view_->GetWidget()->SetBounds(gfx::Rect(root->bounds().size()));
}

void AppList::OnWindowFocused(aura::Window* window) {
  if (view_ && is_visible_) {
    aura::Window* applist_container = Shell::GetInstance()->GetContainer(
        internal::kShellWindowId_AppListContainer);
    aura::Window* bubble_container = Shell::GetInstance()->GetContainer(
        internal::kShellWindowId_SettingBubbleContainer);
    if (window->parent() != applist_container &&
        window->parent() != bubble_container) {
      SetVisible(false);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppList, ui::ImplicitAnimationObserver implementation:

void AppList::OnImplicitAnimationsCompleted() {
  if (is_visible_ )
    view_->GetWidget()->Activate();
  else
    view_->GetWidget()->Close();
}

////////////////////////////////////////////////////////////////////////////////
// AppList, views::Widget::Observer implementation:

void AppList::OnWidgetClosing(views::Widget* widget) {
  DCHECK(view_->GetWidget() == widget);
  if (is_visible_)
    SetVisible(false);
  ResetView();
}

}  // namespace internal
}  // namespace ash
