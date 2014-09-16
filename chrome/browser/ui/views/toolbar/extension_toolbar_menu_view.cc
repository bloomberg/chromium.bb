// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/extension_toolbar_menu_view.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/wrench_menu.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace {

// Returns the padding before the BrowserActionsContainer in the menu.
int start_padding() {
  // We pad enough on the left so that the first icon starts at the same point
  // as the labels. We need to subtract 1 because we want the pixel *before*
  // the label, and we subtract kItemSpacing because there needs to be padding
  // so we can see the drop indicator.
  return views::MenuItemView::label_start() - 1 -
      BrowserActionsContainer::kItemSpacing;
}

}  // namespace

ExtensionToolbarMenuView::ExtensionToolbarMenuView(Browser* browser,
                                                   WrenchMenu* wrench_menu)
    : browser_(browser),
      wrench_menu_(wrench_menu),
      container_(NULL),
      browser_actions_container_observer_(this),
      weak_factory_(this) {
  BrowserActionsContainer* main =
      BrowserView::GetBrowserViewForBrowser(browser_)
          ->toolbar()->browser_actions();
  container_ = new BrowserActionsContainer(
      browser_,
      NULL,  // No owner view, means no extra keybindings are registered.
      main);
  container_->Init();
  AddChildView(container_);
  // We Layout() the container here so that we know the number of actions
  // that will be visible in ShouldShow().
  container_->Layout();

  // If we were opened for a drop command, we have to wait for the drop to
  // finish so we can close the wrench menu.
  if (wrench_menu_->for_drop()) {
    browser_actions_container_observer_.Add(container_);
    browser_actions_container_observer_.Add(main);
  }
}

ExtensionToolbarMenuView::~ExtensionToolbarMenuView() {
}

bool ExtensionToolbarMenuView::ShouldShow() {
  return wrench_menu_->for_drop() ||
      container_->VisibleBrowserActionsAfterAnimation();
}

gfx::Size ExtensionToolbarMenuView::GetPreferredSize() const {
  return container_->GetPreferredSize();
}

int ExtensionToolbarMenuView::GetHeightForWidth(int width) const {
  const views::MenuConfig& menu_config =
      static_cast<const views::MenuItemView*>(parent())->GetMenuConfig();
  int end_padding = menu_config.arrow_to_edge_padding -
                    BrowserActionsContainer::kItemSpacing;
  width -= start_padding() + end_padding;

  int height = container_->GetHeightForWidth(width);
  return height;
}

void ExtensionToolbarMenuView::Layout() {
  gfx::Size sz = GetPreferredSize();
  SetBounds(start_padding() + 1, 0, sz.width(), sz.height());
  container_->SetBounds(0, 0, sz.width(), sz.height());
}

void ExtensionToolbarMenuView::OnBrowserActionDragDone() {
  // The delay before we close the wrench menu if this was opened for a drop so
  // that the user can see a browser action if one was moved.
  static const int kCloseMenuDelay = 300;

  DCHECK(wrench_menu_->for_drop());
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ExtensionToolbarMenuView::CloseWrenchMenu,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kCloseMenuDelay));
}

void ExtensionToolbarMenuView::CloseWrenchMenu() {
  wrench_menu_->CloseMenu();
}
