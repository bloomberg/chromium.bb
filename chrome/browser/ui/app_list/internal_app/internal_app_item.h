// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_ITEM_H_
#define CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_ITEM_H_

#include "base/macros.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"

namespace app_list {
struct InternalApp;
}

class InternalAppItem : public ChromeAppListItem {
 public:
  static const char kItemType[];

  InternalAppItem(Profile* profile,
                  const app_list::AppListSyncableService::SyncItem* sync_item,
                  const app_list::InternalApp& internal_app);

  ~InternalAppItem() override = default;

 private:
  // ChromeAppListItem:
  void Activate(int event_flags) override;
  const char* GetItemType() const override;

  DISALLOW_COPY_AND_ASSIGN(InternalAppItem);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_ITEM_H_
