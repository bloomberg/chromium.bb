// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_APP_ITEM_H_
#define CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_APP_ITEM_H_

#include "chrome/browser/ui/app_list/chrome_app_list_item.h"

namespace gfx {
class ImageSkia;
}

class CrostiniAppItem : public ChromeAppListItem {
 public:
  static const char kItemType[];
  CrostiniAppItem(Profile* profile,
                  const app_list::AppListSyncableService::SyncItem* sync_item,
                  const std::string& id,
                  const std::string& name,
                  const gfx::ImageSkia* image_skia);
  ~CrostiniAppItem() override;

 private:
  // ChromeAppListItem:
  void Activate(int event_flags) override;
  const char* GetItemType() const override;

  DISALLOW_COPY_AND_ASSIGN(CrostiniAppItem);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_APP_ITEM_H_
