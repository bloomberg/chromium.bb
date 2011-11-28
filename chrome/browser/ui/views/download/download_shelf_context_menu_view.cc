// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "chrome/browser/download/download_item_model.h"
#include "content/browser/download/download_item.h"
#include "ui/gfx/point.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

DownloadShelfContextMenuView::DownloadShelfContextMenuView(
    BaseDownloadItemModel* model)
    : DownloadShelfContextMenu(model) {
    DCHECK(model);
}

DownloadShelfContextMenuView::~DownloadShelfContextMenuView() {}

bool DownloadShelfContextMenuView::Run(views::Widget* parent_widget,
                                       const gfx::Rect& rect) {
  views::MenuModelAdapter menu_model_adapter(GetMenuModel());
  menu_runner_.reset(new views::MenuRunner(menu_model_adapter.CreateMenu()));

  // The menu's alignment is determined based on the UI layout.
  views::MenuItemView::AnchorPosition position;
  if (base::i18n::IsRTL())
    position = views::MenuItemView::TOPRIGHT;
  else
    position = views::MenuItemView::TOPLEFT;
  return menu_runner_->RunMenuAt(
      parent_widget,
      NULL,
      rect,
      position,
      views::MenuRunner::HAS_MNEMONICS) == views::MenuRunner::MENU_DELETED;
}

void DownloadShelfContextMenuView::Stop() {
  set_download_item(NULL);
}
