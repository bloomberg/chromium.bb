// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list.h"

#include "ash/app_list/app_list_view.h"
#include "ash/shell_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform_util.h"

namespace ash {
namespace internal {

namespace {

const float kDefaultContainerAnimationScaleFactor = 1.05f;

// Gets preferred bounds of app list window.
gfx::Rect GetPreferredBounds() {
  gfx::Point cursor = gfx::Screen::GetCursorScreenPoint();
  // Use full monitor rect so that the app list shade goes behind the launcher.
  return gfx::Screen::GetMonitorAreaNearestPoint(cursor);
}

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppList, public:

AppList::AppList() : is_visible_(false), view_(NULL) {
}

AppList::~AppList() {
  ResetView();
}

void AppList::SetVisible(bool visible) {
  if (visible == is_visible_)
    return;

  is_visible_ = visible;

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

////////////////////////////////////////////////////////////////////////////////
// AppList, private:

void AppList::SetView(AppListView* view) {
  DCHECK(view_ == NULL);

  if (is_visible_) {
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
}

void AppList::ScheduleAnimation() {
  aura::Window* default_container = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_DefaultContainer);
  // |default_container| could be NULL during Shell shutdown.
  if (!default_container)
    return;

  ui::Layer* layer = GetLayer(view_->GetWidget());

  // Stop observing previous animation.
  StopObservingImplicitAnimations();

  ui::ScopedLayerAnimationSettings app_list_animation(layer->GetAnimator());
  app_list_animation.AddObserver(this);
  layer->SetOpacity(is_visible_ ? 1.0 : 0.0);

  if (is_visible_)
    view_->AnimateShow();
  else
    view_->AnimateHide();

  ui::Layer* default_container_layer = default_container->layer();
  ui::ScopedLayerAnimationSettings default_container_animation(
      default_container_layer->GetAnimator());
  app_list_animation.AddObserver(this);
  default_container_layer->SetOpacity(is_visible_ ? 0.0 : 1.0);
  default_container_layer->SetTransform(is_visible_ ?
      ui::GetScaleTransform(
          gfx::Point(default_container_layer->bounds().width() / 2,
                     default_container_layer->bounds().height() / 2),
          kDefaultContainerAnimationScaleFactor) :
      ui::Transform());
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
void AppList::OnRootWindowResized(const gfx::Size& new_size) {
  if (view_&& is_visible_)
    view_->GetWidget()->SetBounds(gfx::Rect(new_size));
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

void AppList::OnWidgetActivationChanged(views::Widget* widget, bool active) {
  DCHECK(view_->GetWidget() == widget);
  if (view_ && is_visible_ && !active)
    SetVisible(false);
}

}  // namespace internal
}  // namespace ash
