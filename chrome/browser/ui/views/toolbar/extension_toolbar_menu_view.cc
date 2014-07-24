// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/extension_toolbar_menu_view.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace {

// Bottom padding to make sure we have enough room for the icons.
// TODO(devlin): Figure out why the bottom few pixels of the last row in the
// overflow menu are cut off (so we can remove this).
const int kVerticalPadding = 8;

}  // namespace

ExtensionToolbarMenuView::ExtensionToolbarMenuView(Browser* browser)
    : browser_(browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  container_ = new BrowserActionsContainer(
      browser_,
      NULL,  // No owner view, means no extra keybindings are registered.
      browser_view->GetToolbarView()->browser_actions());
  container_->Init();
  AddChildView(container_);
}

ExtensionToolbarMenuView::~ExtensionToolbarMenuView() {
}

gfx::Size ExtensionToolbarMenuView::GetPreferredSize() const {
  gfx::Size sz = container_->GetPreferredSize();
  if (sz.height() == 0)
    return sz;

  sz.Enlarge(0, kVerticalPadding);
  return sz;
}

void ExtensionToolbarMenuView::Layout() {
  // All buttons are given the same width.
  gfx::Size sz = container_->GetPreferredSize();
  int height = sz.height() + kVerticalPadding / 2;
  SetBounds(views::MenuItemView::label_start(), 0, sz.width(), height);
  container_->SetBounds(0, 0, sz.width(), height);
}
