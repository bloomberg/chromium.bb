// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/download/download_shelf_context_menu.h"

class BaseDownloadItemModel;

namespace gfx {
class Point;
}

namespace views {
class Menu2;
}

class DownloadShelfContextMenuView : public DownloadShelfContextMenu {
 public:
  explicit DownloadShelfContextMenuView(BaseDownloadItemModel* model);
  virtual ~DownloadShelfContextMenuView();

  void Run(const gfx::Point& point);

  // This method runs when the caller has been deleted and we should not attempt
  // to access download_item().
  void Stop();

 private:
  scoped_ptr<views::Menu2> menu_;

  DISALLOW_COPY_AND_ASSIGN(DownloadShelfContextMenuView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_VIEW_H_
