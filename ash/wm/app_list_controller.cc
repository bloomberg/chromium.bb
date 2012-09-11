// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/app_list_controller.h"

#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/property_util.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ui/app_list/app_list_view.h"
#include "ui/app_list/pagination_model.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {

// Duration for show/hide animation in milliseconds.
const int kAnimationDurationMs = 200;

// Offset in pixels to animation away/towards the launcher.
const int kAnimationOffset = 8;

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

// Gets arrow location based on shelf alignment.
views::BubbleBorder::ArrowLocation GetBubbleArrowLocation() {
  DCHECK(Shell::HasInstance());
  ShelfAlignment shelf_alignment = Shell::GetInstance()->GetShelfAlignment();
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

// Offset given |rect| towards shelf.
gfx::Rect OffsetTowardsShelf(const gfx::Rect& rect) {
  DCHECK(Shell::HasInstance());
  ShelfAlignment shelf_alignment = Shell::GetInstance()->GetShelfAlignment();
  gfx::Rect offseted(rect);
  switch (shelf_alignment) {
    case SHELF_ALIGNMENT_BOTTOM:
      offseted.Offset(0, kAnimationOffset);
      break;
    case SHELF_ALIGNMENT_LEFT:
      offseted.Offset(-kAnimationOffset, 0);
      break;
    case SHELF_ALIGNMENT_RIGHT:
      offseted.Offset(kAnimationOffset, 0);
      break;
    default:
      NOTREACHED() << "Unknown shelf alignment " << shelf_alignment;
      break;
  }

  return offseted;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListController, public:

AppListController::AppListController()
    : pagination_model_(new app_list::PaginationModel),
      is_visible_(false),
      view_(NULL) {
  Shell::GetInstance()->AddShellObserver(this);
}

AppListController::~AppListController() {
  // Ensures app list view goes before the controller since pagination model
  // lives in the controller and app list view would access it on destruction.
  if (view_ && view_->GetWidget())
    view_->GetWidget()->CloseNow();

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
        Shell::GetPrimaryRootWindowController()->GetContainer(
            kShellWindowId_AppListContainer),
        pagination_model_.get(),
        Shell::GetInstance()->launcher()->GetAppListButtonView(),
        gfx::Point(),
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
    view_ = view;
    views::Widget* widget = view_->GetWidget();
    widget->AddObserver(this);
    Shell::GetInstance()->AddEnvEventFilter(this);
    Shell::GetInstance()->launcher()->AddIconObserver(this);
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
  Shell::GetInstance()->launcher()->RemoveIconObserver(this);
  widget->GetNativeView()->GetRootWindow()->RemoveRootWindowObserver(this);
  widget->GetNativeView()->GetFocusManager()->RemoveObserver(this);
  view_ = NULL;
}

void AppListController::ScheduleAnimation() {
  // Stop observing previous animation.
  StopObservingImplicitAnimations();

  ui::Layer* layer = GetLayer(view_->GetWidget());
  layer->GetAnimator()->StopAnimating();

  gfx::Rect target_bounds;
  if (is_visible_) {
    target_bounds = layer->bounds();
    layer->SetBounds(OffsetTowardsShelf(layer->bounds()));
  } else {
    target_bounds = OffsetTowardsShelf(layer->bounds());
  }

  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
  animation.AddObserver(this);

  layer->SetOpacity(is_visible_ ? 1.0 : 0.0);
  layer->SetBounds(target_bounds);
}

void AppListController::ProcessLocatedEvent(aura::Window* target,
                                            const ui::LocatedEvent& event) {
  // If the event happened on a menu, then the event should not close the app
  // list.
  if (target) {
    RootWindowController* root_controller =
        GetRootWindowController(target->GetRootWindow());
    if (root_controller) {
      aura::Window* menu_container = root_controller->GetContainer(
          ash::internal::kShellWindowId_MenuContainer);
      if (menu_container->Contains(target))
        return;
    }
  }

  if (view_ && is_visible_) {
    views::Widget* widget = view_->GetWidget();
    if (!widget->GetNativeView()->GetBoundsInRootWindow().Contains(
        event.root_location())) {
      SetVisible(false);
    }
  }
}

void AppListController::UpdateBounds() {
  if (view_ && is_visible_)
    view_->UpdateBounds();
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, aura::EventFilter implementation:

bool AppListController::PreHandleKeyEvent(aura::Window* target,
                                          ui::KeyEvent* event) {
  return false;
}

bool AppListController::PreHandleMouseEvent(aura::Window* target,
                                            ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessLocatedEvent(target, *event);
  return false;
}

ui::TouchStatus AppListController::PreHandleTouchEvent(
    aura::Window* target,
    ui::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult AppListController::PreHandleGestureEvent(
    aura::Window* target,
    ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP)
    ProcessLocatedEvent(target, *event);
  return ui::ER_UNHANDLED;
}

////////////////////////////////////////////////////////////////////////////////
// AppListController,  aura::FocusObserver implementation:
void AppListController::OnWindowFocused(aura::Window* window) {
  if (view_ && is_visible_) {
    aura::Window* applist_container = Shell::GetContainer(
        Shell::GetPrimaryRootWindow(),
        kShellWindowId_AppListContainer);
    if (window->parent() != applist_container)
      SetVisible(false);
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListController,  aura::RootWindowObserver implementation:
void AppListController::OnRootWindowResized(const aura::RootWindow* root,
                                            const gfx::Size& old_size) {
  UpdateBounds();
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
// AppListController, views::WidgetObserver implementation:

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

////////////////////////////////////////////////////////////////////////////////
// AppListController, LauncherIconObserver implementation:

void AppListController::OnLauncherIconPositionsChanged() {
  UpdateBounds();
}

}  // namespace internal
}  // namespace ash
