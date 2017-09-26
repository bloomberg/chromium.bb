// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_list/app_list_presenter_delegate_mus.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"
#include "ui/app_list/presenter/app_list_view_delegate_factory.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/pointer_watcher_event_router.h"
#include "ui/wm/core/window_util.h"

namespace {

// Gets the point at the center of the display. The calculation should exclude
// the virtual keyboard area. If the height of the display area is less than
// |minimum_height|, its bottom will be extended to that height (so that the
// app list never starts above the top of the screen).
gfx::Point GetCenterOfDisplay(int64_t display_id, int minimum_height) {
  // TODO(mfomitchev): account for virtual keyboard.
  std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  auto it = std::find_if(displays.begin(), displays.end(),
                         [display_id](const display::Display& display) {
                           return display.id() == display_id;
                         });
  DCHECK(it != displays.end());
  gfx::Rect bounds = it->bounds();

  // Apply the |minimum_height|.
  if (bounds.height() < minimum_height)
    bounds.set_height(minimum_height);

  return bounds.CenterPoint();
}

}  // namespace

AppListPresenterDelegateMus::AppListPresenterDelegateMus(
    app_list::AppListPresenterImpl* presenter,
    app_list::AppListViewDelegateFactory* view_delegate_factory)
    : presenter_(presenter), view_delegate_factory_(view_delegate_factory) {}

AppListPresenterDelegateMus::~AppListPresenterDelegateMus() {
  // Presenter is supposed to be dismissed when the delegate is destroyed. This
  // means we don't have to worry about unregistering the PointerWatcher here.
  DCHECK(!presenter_->GetTargetVisibility());
}

app_list::AppListViewDelegate* AppListPresenterDelegateMus::GetViewDelegate() {
  return view_delegate_factory_->GetDelegate();
}

void AppListPresenterDelegateMus::Init(app_list::AppListView* view,
                                       int64_t display_id,
                                       int current_apps_page) {
  view_ = view;
  app_list::AppListView::InitParams params;
  // |parent| is null because chrome cannot directly access ash window
  // containers.
  params.parent_container_id = ash::kShellWindowId_AppListContainer;
  params.initial_apps_page = current_apps_page;
  // TODO(crbug.com/726838): Tablet mode.
  // TODO(crbug.com/768437): Rework the open animation for mash support. See
  // AppListView. For now, force "side shelf" mode, which opens the app list
  // full screen and skips the problematic animations.
  params.is_side_shelf = true;
  view->Initialize(params);

  view->MaybeSetAnchorPoint(
      GetCenterOfDisplay(display_id, GetMinimumBoundsHeightForAppList(view)));

  // TODO(mfomitchev): Setup updating bounds on keyboard bounds change.
  // TODO(mfomitchev): Setup dismissing on maximize (touch-view) mode start/end.
  // TODO(mfomitchev): Setup DnD.
  // TODO(mfomitchev): UpdateAutoHideState for shelf
}

void AppListPresenterDelegateMus::OnShown(int64_t display_id) {
  views::MusClient::Get()->pointer_watcher_event_router()->AddPointerWatcher(
      this, false);
  DCHECK(presenter_->GetTargetVisibility());
}

void AppListPresenterDelegateMus::OnDismissed() {
  views::MusClient::Get()->pointer_watcher_event_router()->RemovePointerWatcher(
      this);
  DCHECK(!presenter_->GetTargetVisibility());
}

void AppListPresenterDelegateMus::UpdateBounds() {
  if (!view_ || !presenter_->GetTargetVisibility())
    return;

  view_->UpdateBounds();
}

gfx::Vector2d AppListPresenterDelegateMus::GetVisibilityAnimationOffset(
    aura::Window* root_window) {
  // TODO(mfomitchev): Classic ash does different animation here depending on
  // shelf alignment. We should probably do that too.
  return gfx::Vector2d(0, kAnimationOffset);
}

base::TimeDelta AppListPresenterDelegateMus::GetVisibilityAnimationDuration(
    aura::Window* root_window,
    bool is_visible) {
  // TODO(crbug.com/752695): Classic ash does a different animation here
  // depending on shelf alignment. We should probably do that too.
  return is_visible ? base::TimeDelta::FromMilliseconds(0)
                    : animation_duration();
}

void AppListPresenterDelegateMus::OnPointerEventObserved(
    const ui::PointerEvent& event,
    const gfx::Point& location_in_screen,
    gfx::NativeView target) {
  // Looks for touch pressed and pointer down events outside the app list.
  if (event.type() != ui::ET_TOUCH_PRESSED &&
      event.type() != ui::ET_POINTER_DOWN) {
    return;
  }

  // Bail if there is no app list, or if the event targets the app list.
  views::Widget* target_widget =
      views::Widget::GetTopLevelWidgetForNativeView(target);
  views::Widget* app_list_widget = view_ ? view_->GetWidget() : nullptr;
  if (!app_list_widget || target_widget == app_list_widget)
    return;

  // Bail if the event targets a context menu in the app list window. In mash,
  // app list context menus are configured such that the parent of the app list
  // window is the transient parent of the context menu window's parent. Yikes!
  aura::Window* app_list = app_list_widget->GetNativeWindow();
  if (app_list && app_list->parent() && target && target->parent() &&
      wm::HasTransientAncestor(target->parent(), app_list->parent())) {
    return;
  }

  // Dismiss the app list for all other event targets, including null.
  presenter_->Dismiss();
}
