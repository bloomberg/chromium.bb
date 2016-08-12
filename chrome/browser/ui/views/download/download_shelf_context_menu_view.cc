// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "chrome/browser/download/download_item_model.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/page_navigator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

DownloadShelfContextMenuView::DownloadShelfContextMenuView(
    content::DownloadItem* download_item)
    : DownloadShelfContextMenu(download_item) {
}

DownloadShelfContextMenuView::~DownloadShelfContextMenuView() {}

void DownloadShelfContextMenuView::Run(views::Widget* parent_widget,
                                       const gfx::Rect& rect,
                                       ui::MenuSourceType source_type) {
  ui::MenuModel* menu_model = GetMenuModel();
  // Run() should not be getting called if the DownloadItem was destroyed.
  DCHECK(menu_model);

  menu_model_adapter_.reset(new views::MenuModelAdapter(
      menu_model, base::Bind(&DownloadShelfContextMenuView::OnMenuClosed,
                             base::Unretained(this))));

  menu_runner_.reset(new views::MenuRunner(menu_model_adapter_->CreateMenu(),
                                           views::MenuRunner::HAS_MNEMONICS |
                                               views::MenuRunner::CONTEXT_MENU |
                                               views::MenuRunner::ASYNC));

  // The menu's alignment is determined based on the UI layout.
  views::MenuAnchorPosition position;
  if (base::i18n::IsRTL())
    position = views::MENU_ANCHOR_TOPRIGHT;
  else
    position = views::MENU_ANCHOR_TOPLEFT;

  menu_runner_->RunMenuAt(parent_widget, NULL, rect, position, source_type);
}

void DownloadShelfContextMenuView::OnMenuClosed() {
  close_time_ = base::TimeTicks::Now();
  menu_model_adapter_.reset();
  menu_runner_.reset();
}
