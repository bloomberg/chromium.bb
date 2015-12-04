// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_ITEM_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_ITEM_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "ui/app_list/app_list_item.h"

namespace content {
class BrowserContext;
}  // content

// ArcAppItem represents an ARC app in app list.
class ArcAppItem : public app_list::AppListItem,
                   public ArcAppIcon::Observer {
 public:
  static const char kItemType[];

  ArcAppItem(content::BrowserContext* context,
             const app_list::AppListSyncableService::SyncItem* sync_item,
             const std::string& id,
             const std::string& name,
             bool ready);
  ~ArcAppItem() override;

  void SetName(const std::string& name);

  void Activate(int event_flags) override;
  const char* GetItemType() const override;

  ArcAppIcon* arc_app_icon() { return arc_app_icon_.get(); }

  bool ready() const { return ready_; }

  void SetReady(bool ready);

  // ArcAppIcon::Observer
  void OnIconUpdated() override;

 private:
  // Updates the app item's icon, if necessary making it gray.
  void UpdateIcon();

  content::BrowserContext* context_;
  bool ready_;
  scoped_ptr<ArcAppIcon> arc_app_icon_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppItem);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_ITEM_H_
