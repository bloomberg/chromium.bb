// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/app_list_controller.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ui/app_list/app_list_view.h"
#include "ui/app_list/icon_cache.h"
#include "ui/aura/event.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform_util.h"

namespace ash {
namespace internal {

namespace {

const float kContainerAnimationScaleFactor = 1.05f;

// Duration for show/hide animation in milliseconds.
const int kAnimationDurationMs = 150;

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

// Gets arrow location based on shelf alignment.
views::BubbleBorder::ArrowLocation GetBubbleArrowLocation() {
  DCHECK(Shell::HasInstance());
  ash::ShelfAlignment shelf_alignment =
      Shell::GetInstance()->shelf()->alignment();
  switch (shelf_alignment) {
    case ash::SHELF_ALIGNMENT_BOTTOM:
      return views::BubbleBorder::BOTTOM_RIGHT;
    case ash::SHELF_ALIGNMENT_LEFT:
      return views::BubbleBorder::LEFT_BOTTOM;
    case ash::SHELF_ALIGNMENT_RIGHT:
      return views::BubbleBorder::RIGHT_BOTTOM;
    default:
      NOTREACHED() << "Unknown shelf alignment " << shelf_alignment;
      return views::BubbleBorder::BOTTOM_RIGHT;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListController, public:

AppListController::AppListController() : is_visible_(false), view_(NULL) {
  app_list::IconCache::CreateInstance();
  Shell::GetInstance()->AddShellObserver(this);
}

AppListController::~AppListController() {
  ResetView();
  app_list::IconCache::DeleteInstance();
  Shell::GetInstance()->RemoveShellObserver(this);
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
    view->InitAsBubble(
        Shell::GetInstance()->GetContainer(kShellWindowId_AppListContainer),
        Shell::GetInstance()->launcher()->GetAppListButtonView(),
        GetBubbleArrowLocation());
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
    Shell::GetInstance()->AddEnvEventFilter(this);
    widget->GetNativeView()->GetRootWindow()->AddRootWindowObserver(this);
    widget->GetNativeView()->GetFocusManager()->AddObserver(this);
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
  Shell::GetInstance()->RemoveEnvEventFilter(this);
  widget->GetNativeView()->GetRootWindow()->RemoveRootWindowObserver(this);
  widget->GetNativeView()->GetFocusManager()->RemoveObserver(this);
  view_ = NULL;

  app_list::IconCache::GetInstance()->PurgeAllUnused();
}

void AppListController::ScheduleAnimation() {
  second_animation_timer_.Stop();

  // Stop observing previous animation.
  StopObservingImplicitAnimations();

  ui::Layer* layer = GetLayer(view_->GetWidget());
  layer->GetAnimator()->StopAnimating();

  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
  animation.AddObserver(this);

  layer->SetOpacity(is_visible_ ? 1.0 : 0.0);
}

void AppListController::ProcessLocatedEvent(const aura::LocatedEvent& event) {
  if (view_ && is_visible_) {
    views::Widget* widget = view_->GetWidget();
    if (!widget->GetNativeView()->GetBoundsInRootWindow().Contains(
        event.root_location())) {
      SetVisible(false);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, aura::EventFilter implementation:

bool AppListController::PreHandleKeyEvent(aura::Window* target,
                                          aura::KeyEvent* event) {
  return false;
}

bool AppListController::PreHandleMouseEvent(aura::Window* target,
                                            aura::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessLocatedEvent(*event);
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
  if (event->type() == ui::ET_GESTURE_TAP)
    ProcessLocatedEvent(*event);
  return ui::GESTURE_STATUS_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
// AppListController,  aura::FocusObserver implementation:
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
// AppListController,  aura::RootWindowObserver implementation:
void AppListController::OnRootWindowResized(const aura::RootWindow* root,
                                            const gfx::Size& old_size) {
  if (view_ && is_visible_)
    view_->UpdateBounds();
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

////////////////////////////////////////////////////////////////////////////////
// AppListController, ShellObserver implementation:
void AppListController::OnShelfAlignmentChanged() {
  if (view_)
    view_->SetBubbleArrowLocation(GetBubbleArrowLocation());
}

}  // namespace internal
}  // namespace ash
