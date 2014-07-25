// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/extension_toolbar_menu_view.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/wrench_menu.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace {

// Bottom padding to make sure we have enough room for the icons.
// TODO(devlin): Figure out why the bottom few pixels of the last row in the
// overflow menu are cut off (so we can remove this).
const int kVerticalPadding = 8;

}  // namespace

ExtensionToolbarMenuView::ExtensionToolbarMenuView(Browser* browser,
                                                   WrenchMenu* wrench_menu)
    : browser_(browser),
      wrench_menu_(wrench_menu),
      container_(NULL),
      browser_actions_container_observer_(this) {
  BrowserActionsContainer* main =
      BrowserView::GetBrowserViewForBrowser(browser_)
          ->toolbar()->browser_actions();
  container_ = new BrowserActionsContainer(
      browser_,
      NULL,  // No owner view, means no extra keybindings are registered.
      main);
  container_->Init();
  AddChildView(container_);

  // If we were opened for a drop command, we have to wait for the drop to
  // finish so we can close the wrench menu.
  if (wrench_menu_->for_drop()) {
    browser_actions_container_observer_.Add(container_);
    browser_actions_container_observer_.Add(main);
  }
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

void ExtensionToolbarMenuView::OnBrowserActionDragDone() {
  DCHECK(wrench_menu_->for_drop());
  wrench_menu_->CloseMenu();
}
