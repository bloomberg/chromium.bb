// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_container_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/location_bar/find_bar_icon.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/page_action/zoom_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "ui/views/layout/box_layout.h"

PageActionIconContainerView::PageActionIconContainerView(
    const std::vector<PageActionIconType>& types_enabled,
    int icon_size,
    int between_icon_spacing,
    Browser* browser,
    CommandUpdater* command_updater,
    PageActionIconView::Delegate* page_action_icon_delegate,
    LocationBarView::Delegate* location_bar_delegate)
    : zoom_observer_(this) {
  views::BoxLayout& layout =
      *SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal, gfx::Insets(), between_icon_spacing));
  // Right align to clip the leftmost items first when not enough space.
  layout.set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);

  for (PageActionIconType type : types_enabled) {
    switch (type) {
      case PageActionIconType::kFind:
        find_bar_icon_ = new FindBarIcon(browser, page_action_icon_delegate);
        page_action_icons_.push_back(find_bar_icon_);
        break;
      case PageActionIconType::kManagePasswords:
        manage_passwords_icon_ = new ManagePasswordsIconViews(
            command_updater, page_action_icon_delegate);
        page_action_icons_.push_back(manage_passwords_icon_);
        break;
      case PageActionIconType::kZoom:
        zoom_view_ =
            new ZoomView(location_bar_delegate, page_action_icon_delegate);
        page_action_icons_.push_back(zoom_view_);
        break;
    }
  }

  for (PageActionIconView* icon : page_action_icons_) {
    icon->SetVisible(false);
    icon->set_icon_size(icon_size);
    icon->Init();
    AddChildView(icon);
  }

  if (browser) {
    zoom_observer_.Add(
        zoom::ZoomEventManager::GetForBrowserContext(browser->profile()));
  }
}

PageActionIconContainerView::~PageActionIconContainerView() {}

PageActionIconView* PageActionIconContainerView::GetPageActionIconView(
    PageActionIconType type) {
  // TODO(https://crbug.com/788051): Update page action icons here as update
  // methods are migrated out of LocationBar to the PageActionIconContainer
  // interface.
  switch (type) {
    case PageActionIconType::kFind:
      return find_bar_icon_;
    case PageActionIconType::kManagePasswords:
      return manage_passwords_icon_;
    case PageActionIconType::kZoom:
      return zoom_view_;
  }
  return nullptr;
}

void PageActionIconContainerView::UpdateAll() {
  for (PageActionIconView* icon : page_action_icons_)
    icon->Update();
}

void PageActionIconContainerView::UpdatePageActionIcon(
    PageActionIconType type) {
  PageActionIconView* icon = GetPageActionIconView(type);
  if (icon)
    icon->Update();
}

bool PageActionIconContainerView::
    ActivateFirstInactiveBubbleForAccessibility() {
  for (PageActionIconView* icon : page_action_icons_) {
    if (!icon->visible() || !icon->GetBubble())
      continue;

    views::Widget* widget = icon->GetBubble()->GetWidget();
    if (widget && widget->IsVisible() && !widget->IsActive()) {
      widget->Show();
      return true;
    }
  }
  return false;
}

void PageActionIconContainerView::SetIconColor(SkColor icon_color) {
  for (PageActionIconView* icon : page_action_icons_)
    icon->SetIconColor(icon_color);
}

void PageActionIconContainerView::ZoomChangedForActiveTab(
    bool can_show_bubble) {
  if (zoom_view_)
    zoom_view_->ZoomChangedForActiveTab(can_show_bubble);
}

void PageActionIconContainerView::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void PageActionIconContainerView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

void PageActionIconContainerView::OnDefaultZoomLevelChanged() {
  ZoomChangedForActiveTab(false);
}
