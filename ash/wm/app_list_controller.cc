// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/app_list_controller.h"

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "base/command_line.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/gfx/transform_util.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Duration for show/hide animation in milliseconds.
const int kAnimationDurationMs = 200;

// Offset in pixels to animation away/towards the shelf.
const int kAnimationOffset = 8;

// The maximum shift in pixels when over-scroll happens.
const int kMaxOverScrollShift = 48;

// The minimal anchor position offset to make sure that the bubble is still on
// the screen with 8 pixels spacing on the left / right. This constant is a
// result of minimal bubble arrow sizes and offsets.
const int kMinimalAnchorPositionOffset = 57;

// The minimal margin (in pixels) around the app list when in centered mode.
const int kMinimalCenteredAppListMargin = 10;

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

// Gets arrow location based on shelf alignment.
views::BubbleBorder::Arrow GetBubbleArrow(aura::Window* window) {
  DCHECK(Shell::HasInstance());
  return ShelfLayoutManager::ForShelf(window)->
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

// Using |button_bounds|, determine the anchor offset so that the bubble gets
// shown above the shelf (used for the alternate shelf theme).
gfx::Vector2d GetAnchorPositionOffsetToShelf(
    const gfx::Rect& button_bounds, views::Widget* widget) {
  DCHECK(Shell::HasInstance());
  ShelfAlignment shelf_alignment = Shell::GetInstance()->GetShelfAlignment(
      widget->GetNativeView()->GetRootWindow());
  gfx::Point anchor(button_bounds.CenterPoint());
  switch (shelf_alignment) {
    case SHELF_ALIGNMENT_TOP:
    case SHELF_ALIGNMENT_BOTTOM:
      if (base::i18n::IsRTL()) {
        int screen_width = widget->GetWorkAreaBoundsInScreen().width();
        return gfx::Vector2d(
            std::min(screen_width - kMinimalAnchorPositionOffset - anchor.x(),
                     0), 0);
      }
      return gfx::Vector2d(
          std::max(kMinimalAnchorPositionOffset - anchor.x(), 0), 0);
    case SHELF_ALIGNMENT_LEFT:
      return gfx::Vector2d(
          0, std::max(kMinimalAnchorPositionOffset - anchor.y(), 0));
    case SHELF_ALIGNMENT_RIGHT:
      return gfx::Vector2d(
          0, std::max(kMinimalAnchorPositionOffset - anchor.y(), 0));
    default:
      NOTREACHED();
      return gfx::Vector2d();
  }
}

// Gets the point at the center of the display that a particular view is on.
// This calculation excludes the virtual keyboard area. If the height of the
// display area is less than |minimum_height|, its bottom will be extended to
// that height (so that the app list never starts above the top of the screen).
gfx::Point GetCenterOfDisplayForView(const views::View* view,
                                     int minimum_height) {
  gfx::Rect bounds = Shell::GetScreen()->GetDisplayNearestWindow(
      view->GetWidget()->GetNativeView()).bounds();

  // If the virtual keyboard is active, subtract it from the display bounds, so
  // that the app list is centered in the non-keyboard area of the display.
  // (Note that work_area excludes the keyboard, but it doesn't get updated
  // until after this function is called.)
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller && keyboard_controller->keyboard_visible())
    bounds.Subtract(keyboard_controller->current_keyboard_bounds());

  // Apply the |minimum_height|.
  if (bounds.height() < minimum_height)
    bounds.set_height(minimum_height);

  return bounds.CenterPoint();
}

// Gets the minimum height of the rectangle to center the app list in.
int GetMinimumBoundsHeightForAppList(const app_list::AppListView* app_list) {
  return app_list->bounds().height() + 2 * kMinimalCenteredAppListMargin;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListController, public:

AppListController::AppListController()
    : is_visible_(false),
      is_centered_(false),
      view_(NULL),
      current_apps_page_(-1),
      should_snap_back_(false) {
  Shell::GetInstance()->AddShellObserver(this);
}

AppListController::~AppListController() {
  // Ensures app list view goes before the controller since pagination model
  // lives in the controller and app list view would access it on destruction.
  if (view_) {
    view_->GetAppsPaginationModel()->RemoveObserver(this);
    if (view_->GetWidget())
      view_->GetWidget()->CloseNow();
  }

  Shell::GetInstance()->RemoveShellObserver(this);
}

void AppListController::SetVisible(bool visible, aura::Window* window) {
  if (visible == is_visible_)
    return;

  is_visible_ = visible;

  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager()->
      UpdateAutoHideState();

  if (view_) {
    // Our widget is currently active. When the animation completes we'll hide
    // the widget, changing activation. If a menu is shown before the animation
    // completes then the activation change triggers the menu to close. By
    // deactivating now we ensure there is no activation change when the
    // animation completes and any menus stay open.
    if (!visible)
      view_->GetWidget()->Deactivate();
    ScheduleAnimation();
  } else if (is_visible_) {
    // AppListModel and AppListViewDelegate are owned by AppListView. They
    // will be released with AppListView on close.
    app_list::AppListView* view = new app_list::AppListView(
        Shell::GetInstance()->delegate()->CreateAppListViewDelegate());
    aura::Window* root_window = window->GetRootWindow();
    aura::Window* container = GetRootWindowController(root_window)->
        GetContainer(kShellWindowId_AppListContainer);
    views::View* applist_button =
        Shelf::ForWindow(container)->GetAppListButtonView();
    is_centered_ = view->ShouldCenterWindow();
    if (is_centered_) {
      // Note: We can't center the app list until we have its dimensions, so we
      // init at (0, 0) and then reset its anchor point.
      view->InitAsBubbleAtFixedLocation(container,
                                        current_apps_page_,
                                        gfx::Point(),
                                        views::BubbleBorder::FLOAT,
                                        true /* border_accepts_events */);
      // The experimental app list is centered over the display of the app list
      // button that was pressed (if triggered via keyboard, this is the display
      // with the currently focused window).
      view->SetAnchorPoint(GetCenterOfDisplayForView(
          applist_button, GetMinimumBoundsHeightForAppList(view)));
    } else {
      gfx::Rect applist_button_bounds = applist_button->GetBoundsInScreen();
      // We need the location of the button within the local screen.
      applist_button_bounds = ScreenUtil::ConvertRectFromScreen(
          root_window,
          applist_button_bounds);
      view->InitAsBubbleAttachedToAnchor(
          container,
          current_apps_page_,
          Shelf::ForWindow(container)->GetAppListButtonView(),
          GetAnchorPositionOffsetToShelf(
              applist_button_bounds,
              Shelf::ForWindow(container)->GetAppListButtonView()->GetWidget()),
          GetBubbleArrow(container),
          true /* border_accepts_events */);
      view->SetArrowPaintType(views::BubbleBorder::PAINT_NONE);
    }
    SetView(view);
    // By setting us as DnD recipient, the app list knows that we can
    // handle items.
    SetDragAndDropHostOfCurrentAppList(
        Shelf::ForWindow(window)->GetDragAndDropHostForAppList());
  }
  // Update applist button status when app list visibility is changed.
  Shelf::ForWindow(window)->GetAppListButtonView()->SchedulePaint();
}

bool AppListController::IsVisible() const {
  return view_ && view_->GetWidget()->IsVisible();
}

aura::Window* AppListController::GetWindow() {
  return is_visible_ && view_ ? view_->GetWidget()->GetNativeWindow() : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, private:

void AppListController::SetDragAndDropHostOfCurrentAppList(
    app_list::ApplicationDragAndDropHost* drag_and_drop_host) {
  if (view_ && is_visible_)
    view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void AppListController::SetView(app_list::AppListView* view) {
  DCHECK(view_ == NULL);
  DCHECK(is_visible_);

  view_ = view;
  views::Widget* widget = view_->GetWidget();
  widget->AddObserver(this);
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller)
    keyboard_controller->AddObserver(this);
  Shell::GetInstance()->AddPreTargetHandler(this);
  Shelf::ForWindow(widget->GetNativeWindow())->AddIconObserver(this);
  widget->GetNativeView()->GetRootWindow()->AddObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->AddObserver(this);

  view_->GetAppsPaginationModel()->AddObserver(this);

  view_->ShowWhenReady();
}

void AppListController::ResetView() {
  if (!view_)
    return;

  views::Widget* widget = view_->GetWidget();
  widget->RemoveObserver(this);
  GetLayer(widget)->GetAnimator()->RemoveObserver(this);
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller)
    keyboard_controller->RemoveObserver(this);
  Shell::GetInstance()->RemovePreTargetHandler(this);
  Shelf::ForWindow(widget->GetNativeWindow())->RemoveIconObserver(this);
  widget->GetNativeView()->GetRootWindow()->RemoveObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->RemoveObserver(this);

  view_->GetAppsPaginationModel()->RemoveObserver(this);

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
  if (!view_ || !is_visible_)
    return;

  // If the event happened on a menu, then the event should not close the app
  // list.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (target) {
    RootWindowController* root_controller =
        GetRootWindowController(target->GetRootWindow());
    if (root_controller) {
      aura::Window* menu_container =
          root_controller->GetContainer(kShellWindowId_MenuContainer);
      if (menu_container->Contains(target))
        return;
      aura::Window* keyboard_container = root_controller->GetContainer(
          kShellWindowId_VirtualKeyboardContainer);
      if (keyboard_container->Contains(target))
        return;
    }
  }

  aura::Window* window = view_->GetWidget()->GetNativeView()->parent();
  if (!window->Contains(target))
    SetVisible(false, window);
}

void AppListController::UpdateBounds() {
  if (!view_ || !is_visible_)
    return;

  view_->UpdateBounds();

  if (is_centered_)
    view_->SetAnchorPoint(GetCenterOfDisplayForView(
        view_, GetMinimumBoundsHeightForAppList(view_)));
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, aura::EventFilter implementation:

void AppListController::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessLocatedEvent(event);
}

void AppListController::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN)
    ProcessLocatedEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// AppListController,  aura::FocusObserver implementation:

void AppListController::OnWindowFocused(aura::Window* gained_focus,
                                        aura::Window* lost_focus) {
  if (view_ && is_visible_) {
    aura::Window* applist_window = view_->GetWidget()->GetNativeView();
    aura::Window* applist_container = applist_window->parent();

    if (applist_container->Contains(lost_focus) &&
        (!gained_focus || !applist_container->Contains(gained_focus))) {
      SetVisible(false, applist_window);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListController,  aura::WindowObserver implementation:
void AppListController::OnWindowBoundsChanged(aura::Window* root,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {
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

void AppListController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(view_->GetWidget() == widget);
  if (is_visible_)
    SetVisible(false, widget->GetNativeView());
  ResetView();
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, keyboard::KeyboardControllerObserver implementation:

void AppListController::OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) {
  UpdateBounds();
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, ShellObserver implementation:
void AppListController::OnShelfAlignmentChanged(aura::Window* root_window) {
  if (view_)
    view_->SetBubbleArrow(GetBubbleArrow(view_->GetWidget()->GetNativeView()));
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, ShelfIconObserver implementation:

void AppListController::OnShelfIconPositionsChanged() {
  UpdateBounds();
}

////////////////////////////////////////////////////////////////////////////////
// AppListController, PaginationModelObserver implementation:

void AppListController::TotalPagesChanged() {
}

void AppListController::SelectedPageChanged(int old_selected,
                                            int new_selected) {
  current_apps_page_ = new_selected;
}

void AppListController::TransitionStarted() {
}

void AppListController::TransitionChanged() {
  // |view_| could be NULL when app list is closed with a running transition.
  if (!view_)
    return;

  app_list::PaginationModel* pagination_model = view_->GetAppsPaginationModel();

  const app_list::PaginationModel::Transition& transition =
      pagination_model->transition();
  if (pagination_model->is_valid_page(transition.target_page))
    return;

  views::Widget* widget = view_->GetWidget();
  ui::LayerAnimator* widget_animator = GetLayer(widget)->GetAnimator();
  if (!pagination_model->IsRevertingCurrentTransition()) {
    // Update cached |view_bounds_| if it is the first over-scroll move and
    // widget does not have running animations.
    if (!should_snap_back_ && !widget_animator->is_animating())
      view_bounds_ = widget->GetWindowBoundsInScreen();

    const int current_page = pagination_model->selected_page();
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

}  // namespace ash
