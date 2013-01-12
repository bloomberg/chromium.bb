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
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/client/focus_client.h"
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

// The maximum shift in pixels when over-scroll happens.
const int kMaxOverScrollShift = 48;

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

// Gets arrow location based on shelf alignment.
views::BubbleBorder::ArrowLocation GetBubbleArrowLocation(
    aura::Window* window) {
  DCHECK(Shell::HasInstance());
  return ShelfLayoutManager::ForLauncher(window)->
      SelectValueForShelfAlignment(
          views::BubbleBorder::BOTTOM_CENTER,
          views::BubbleBorder::LEFT_CENTER,
          views::BubbleBorder::RIGHT_CENTER,
          views::BubbleBorder::TOP_CENTER);
}

// Offset given |rect| towards shelf.
gfx::Rect OffsetTowardsShelf(const gfx::Rect& rect, views::Widget* widget) {
  DCHECK(Shell::HasInstance());
  ShelfAlignment shelf_alignment = Shell::GetInstance()->GetShelfAlignment(
      widget->GetNativeView()->GetRootWindow());
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
    case SHELF_ALIGNMENT_TOP:
      offseted.Offset(0, -kAnimationOffset);
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
      view_(NULL),
      should_snap_back_(false) {
  Shell::GetInstance()->AddShellObserver(this);
  pagination_model_->AddObserver(this);
}

AppListController::~AppListController() {
  // Ensures app list view goes before the controller since pagination model
  // lives in the controller and app list view would access it on destruction.
  if (view_ && view_->GetWidget())
    view_->GetWidget()->CloseNow();

  Shell::GetInstance()->RemoveShellObserver(this);
  pagination_model_->RemoveObserver(this);
}

void AppListController::SetVisible(bool visible, aura::Window* window) {
  if (visible == is_visible_)
    return;

  is_visible_ = visible;

  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  Shell::GetPrimaryRootWindowController()->shelf()->UpdateAutoHideState();

  if (view_) {
    ScheduleAnimation();
  } else if (is_visible_) {
    // AppListModel and AppListViewDelegate are owned by AppListView. They
    // will be released with AppListView on close.
    app_list::AppListView* view = new app_list::AppListView(
        Shell::GetInstance()->delegate()->CreateAppListViewDelegate());
    aura::Window* container = GetRootWindowController(window->GetRootWindow())->
        GetContainer(kShellWindowId_AppListContainer);
    view->InitAsBubble(
        container,
        pagination_model_.get(),
        Launcher::ForWindow(container)->GetAppListButtonView(),
        gfx::Point(),
        GetBubbleArrowLocation(container));
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
  DCHECK(is_visible_);

  view_ = view;
  views::Widget* widget = view_->GetWidget();
  widget->AddObserver(this);
  Shell::GetInstance()->AddPreTargetHandler(this);
  Launcher::ForWindow(widget->GetNativeWindow())->AddIconObserver(this);
  widget->GetNativeView()->GetRootWindow()->AddRootWindowObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->AddObserver(this);

  view_->ShowWhenReady();
}

void AppListController::ResetView() {
  if (!view_)
    return;

  views::Widget* widget = view_->GetWidget();
  widget->RemoveObserver(this);
  GetLayer(widget)->GetAnimator()->RemoveObserver(this);
  Shell::GetInstance()->RemovePreTargetHandler(this);
  Launcher::ForWindow(widget->GetNativeWindow())->RemoveIconObserver(this);
  widget->GetNativeView()->GetRootWindow()->RemoveRootWindowObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->RemoveObserver(this);
  view_ = NULL;
}

void AppListController::ScheduleAnimation() {
  // Stop observing previous animation.
  StopObservingImplicitAnimations();

  views::Widget* widget = view_->GetWidget();
  ui::Layer* layer = GetLayer(widget);
  layer->GetAnimator()->StopAnimating();

  gfx::Rect target_bounds;
  if (is_visible_) {
    target_bounds = widget->GetWindowBoundsInScreen();
    widget->SetBounds(OffsetTowardsShelf(target_bounds, widget));
  } else {
    target_bounds = OffsetTowardsShelf(widget->GetWindowBoundsInScreen(),
                                       widget);
  }

  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(
          is_visible_ ? 0 : kAnimationDurationMs));
  animation.AddObserver(this);

  layer->SetOpacity(is_visible_ ? 1.0 : 0.0);
  widget->SetBounds(target_bounds);
}

void AppListController::ProcessLocatedEvent(ui::LocatedEvent* event) {
  // If the event happened on a menu, then the event should not close the app
  // list.
  aura::Window* target = static_cast<aura::Window*>(event->target());
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
    aura::Window* window = view_->GetWidget()->GetNativeView();
    gfx::Point window_local_point(event->root_location());
    aura::Window::ConvertPointToTarget(window->GetRootWindow(),
                                       window,
                                       &window_local_point);
    // Use HitTest to respect the hit test mask of the bubble.
    if (!window->HitTest(window_local_point))
      SetVisible(false, window);
  }
}

void AppListController::UpdateBounds() {
  if (view_ && is_visible_)
    view_->UpdateBounds();
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, aura::EventFilter implementation:

void AppListController::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessLocatedEvent(event);
}

void AppListController::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP)
    ProcessLocatedEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// AppListController,  aura::FocusObserver implementation:

void AppListController::OnWindowFocused(aura::Window* gained_focus,
                                        aura::Window* lost_focus) {
  if (gained_focus && view_ && is_visible_) {
    aura::Window* applist_container =
        GetRootWindowController(gained_focus->GetRootWindow())->GetContainer(
            kShellWindowId_AppListContainer);
    if (gained_focus->parent() != applist_container)
      SetVisible(false, gained_focus);
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
    SetVisible(false, widget->GetNativeView());
  ResetView();
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, ShellObserver implementation:
void AppListController::OnShelfAlignmentChanged(aura::RootWindow* root_window) {
  if (view_) {
    view_->SetBubbleArrowLocation(GetBubbleArrowLocation(
        view_->GetWidget()->GetNativeView()));
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, LauncherIconObserver implementation:

void AppListController::OnLauncherIconPositionsChanged() {
  UpdateBounds();
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, PaginationModelObserver implementation:

void AppListController::TotalPagesChanged() {
}

void AppListController::SelectedPageChanged(int old_selected,
                                            int new_selected) {
}

void AppListController::TransitionChanged() {
  // |view_| could be NULL when app list is closed with a running transition.
  if (!view_)
    return;

  const app_list::PaginationModel::Transition& transition =
      pagination_model_->transition();
  if (pagination_model_->is_valid_page(transition.target_page))
    return;

  views::Widget* widget = view_->GetWidget();
  ui::LayerAnimator* widget_animator = GetLayer(widget)->GetAnimator();
  if (!pagination_model_->IsRevertingCurrentTransition()) {
    // Update cached |view_bounds_| if it is the first over-scroll move and
    // widget does not have running animations.
    if (!should_snap_back_ && !widget_animator->is_animating())
      view_bounds_ = widget->GetWindowBoundsInScreen();

    const int current_page = pagination_model_->selected_page();
    const int dir = transition.target_page > current_page ? -1 : 1;

    const double progress = 1.0 - pow(1.0 - transition.progress, 4);
    const int shift = kMaxOverScrollShift * progress * dir;

    gfx::Rect shifted(view_bounds_);
    shifted.set_x(shifted.x() + shift);
    widget->SetBounds(shifted);
    should_snap_back_ = true;
  } else if (should_snap_back_) {
    should_snap_back_ = false;
    ui::ScopedLayerAnimationSettings animation(widget_animator);
    animation.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        app_list::kOverscrollPageTransitionDurationMs));
    widget->SetBounds(view_bounds_);
  }
}

}  // namespace internal
}  // namespace ash
