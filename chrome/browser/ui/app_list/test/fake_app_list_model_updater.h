// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ui/app_list/app_list_model_updater.h"

namespace app_list {

class FakeAppListModelUpdater : public AppListModelUpdater {
 public:
  FakeAppListModelUpdater();
  ~FakeAppListModelUpdater() override;

  // AppListModelUpdater:
  void AddItem(std::unique_ptr<AppListItem> item) override;
  void RemoveItem(const std::string& id) override;
  void RemoveUninstalledItem(const std::string& id) override;
  AppListItem* FindItem(const std::string& id) override;
  size_t ItemCount() override;
  AppListItem* ItemAt(size_t index) override;

 private:
  bool FindItemIndex(const std::string& id, size_t* index);

  std::vector<std::unique_ptr<AppListItem>> items_;

  DISALLOW_COPY_AND_ASSIGN(FakeAppListModelUpdater);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_
