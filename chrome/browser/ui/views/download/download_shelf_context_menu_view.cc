// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "chrome/browser/download/download_item_model.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/page_navigator.h"
#include "ui/gfx/point.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

DownloadShelfContextMenuView::DownloadShelfContextMenuView(
    content::DownloadItem* download_item,
    content::PageNavigator* navigator)
    : DownloadShelfContextMenu(download_item, navigator) {
}

DownloadShelfContextMenuView::~DownloadShelfContextMenuView() {}

void DownloadShelfContextMenuView::Run(views::Widget* parent_widget,
                                       const gfx::Rect& rect) {
  ui::MenuModel* menu_model = GetMenuModel();
  // Run() should not be getting called if the DownloadItem was destroyed.
  DCHECK(menu_model);

  views::MenuModelAdapter menu_model_adapter(menu_model);
  menu_runner_.reset(new views::MenuRunner(menu_model_adapter.CreateMenu()));

  // The menu's alignment is determined based on the UI layout.
  views::MenuItemView::AnchorPosition position;
  if (base::i18n::IsRTL())
    position = views::MenuItemView::TOPRIGHT;
  else
    position = views::MenuItemView::TOPLEFT;

  // The return value of RunMenuAt indicates whether the MenuRunner was deleted
  // while running the menu, which indicates that the containing view may have
  // been deleted. We ignore the return value because our caller already assumes
  // that the view could be deleted by the time we return from here.
  ignore_result(menu_runner_->RunMenuAt(
      parent_widget,
      NULL,
      rect,
      position,
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));
}
