// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_item_model.h"
#include "ui/gfx/point.h"
#include "views/controls/menu/menu_2.h"

DownloadShelfContextMenuView::DownloadShelfContextMenuView(
    BaseDownloadItemModel* model)
    : DownloadShelfContextMenu(model) {
    DCHECK(model);
}

DownloadShelfContextMenuView::~DownloadShelfContextMenuView() {}

void DownloadShelfContextMenuView::Run(const gfx::Point& point) {
  menu_.reset(new views::Menu2(GetMenuModel()));

  // The menu's alignment is determined based on the UI layout.
  views::Menu2::Alignment alignment;
  if (base::i18n::IsRTL())
    alignment = views::Menu2::ALIGN_TOPRIGHT;
  else
    alignment = views::Menu2::ALIGN_TOPLEFT;
  menu_->RunMenuAt(point, alignment);
}

void DownloadShelfContextMenuView::Stop() {
  set_download_item(NULL);
}
