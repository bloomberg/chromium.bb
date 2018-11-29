// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_ITEM_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_ITEM_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/services/app_service/public/cpp/app_update.h"

class AppServiceAppItem : public ChromeAppListItem {
 public:
  static const char kItemType[];

  AppServiceAppItem(Profile* profile,
                    AppListModelUpdater* model_updater,
                    const app_list::AppListSyncableService::SyncItem* sync_item,
                    const apps::AppUpdate& app_update);
  ~AppServiceAppItem() override;

 private:
  // ChromeAppListItem overrides:
  const char* GetItemType() const override;
  // TODO(crbug.com/826982): Activate, GetContextMenuModel, etc.

  void OnLoadIcon(apps::mojom::IconValuePtr icon_value);

  base::WeakPtrFactory<AppServiceAppItem> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppServiceAppItem);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_ITEM_H_
