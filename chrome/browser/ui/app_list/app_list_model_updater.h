// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_

#include <memory>
#include <string>

#include "ash/app_list/model/app_list_item.h"

namespace app_list {

// An interface to wrap AppListModel access in browser.
class AppListModelUpdater {
 public:
  virtual void AddItem(std::unique_ptr<AppListItem> item) = 0;
  virtual void RemoveItem(const std::string& id) = 0;
  virtual void RemoveUninstalledItem(const std::string& id) = 0;

  // TODO(hejq): Remove this and access the model in a mojo way.
  virtual AppListItem* FindItem(const std::string& id) = 0;
  virtual size_t ItemCount() = 0;
  virtual AppListItem* ItemAt(size_t index) = 0;

 protected:
  virtual ~AppListModelUpdater() {}
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_
