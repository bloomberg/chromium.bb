// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_list/app_list_presenter_delegate_mus.h"

#include "ui/app_list/presenter/app_list_view_delegate_factory.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

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
    app_list::AppListViewDelegateFactory* view_delegate_factory)
    : view_delegate_factory_(view_delegate_factory) {}

AppListPresenterDelegateMus::~AppListPresenterDelegateMus() {}

app_list::AppListViewDelegate* AppListPresenterDelegateMus::GetViewDelegate() {
  return view_delegate_factory_->GetDelegate();
}

void AppListPresenterDelegateMus::Init(app_list::AppListView* view,
                                       int64_t display_id,
                                       int current_apps_page) {
  view_ = view;

  // Note: This would place the app list into the USER_WINDOWS container, unlike
  // in classic ash, where it has it's own container.
  // Note: We can't center the app list until we have its dimensions, so we
  // init at (0, 0) and then reset its anchor point.
  // TODO(mfomitchev): We are currently passing NULL for |parent|. It seems like
  // the only thing this is used for is choosing the right scale factor in
  // AppListMainView::PreloadIcons(), so we take care of that - perhaps by
  // passing the display_id or the scale factor directly
  view->InitAsBubbleAtFixedLocation(nullptr /* parent */, current_apps_page,
                                    gfx::Point(), views::BubbleBorder::FLOAT,
                                    true /* border_accepts_events */);

  view->SetAnchorPoint(
      GetCenterOfDisplay(display_id, GetMinimumBoundsHeightForAppList(view)));

  // TODO(mfomitchev): Setup updating bounds on keyboard bounds change.
  // TODO(mfomitchev): Setup dismissing on mouse/touch gesture anywhere outside
  // the bounds of the app list.
  // TODO(mfomitchev): Setup dismissing on maximize (touch-view) mode start/end.
  // TODO(mfomitchev): Setup DnD.
  // TODO(mfomitchev): UpdateAutoHideState for shelf
}

void AppListPresenterDelegateMus::OnShown(int64_t display_id) {
  is_visible_ = true;
}

void AppListPresenterDelegateMus::OnDismissed() {
  DCHECK(is_visible_);
  is_visible_ = false;
}

void AppListPresenterDelegateMus::UpdateBounds() {
  if (!view_ || !is_visible_)
    return;

  view_->UpdateBounds();
}

gfx::Vector2d AppListPresenterDelegateMus::GetVisibilityAnimationOffset(
    aura::Window* root_window) {
  // TODO(mfomitchev): Classic ash does different animation here depending on
  // shelf alignment. We should probably do that too.
  return gfx::Vector2d(0, kAnimationOffset);
}
