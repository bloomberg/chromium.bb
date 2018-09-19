// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_IMPL_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/android/explore_sites/explore_sites_service.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "components/offline_pages/task/task_queue.h"

using offline_pages::TaskQueue;

namespace explore_sites {
class Catalog;

class ExploreSitesServiceImpl : public ExploreSitesService,
                                public TaskQueue::Delegate {
 public:
  explicit ExploreSitesServiceImpl(std::unique_ptr<ExploreSitesStore> store);
  ~ExploreSitesServiceImpl() override;

  bool IsExploreSitesEnabled();

  // ExploreSitesService implementation.
  void GetCatalog(CatalogCallback callback) override;
  void GetCategoryImage(int category_id, BitmapCallback callback) override;
  void GetSiteImage(int site_id, BitmapCallback callback) override;

 private:
  // KeyedService implementation:
  void Shutdown() override;

  // TaskQueue::Delegate implementation:
  void OnTaskQueueIsIdle() override;

  void AddUpdatedCatalog(int64_t catalog_timestamp,
                         std::unique_ptr<Catalog> catalog_proto);

  // True when Chrome starts up, this is reset after the catalog is requested
  // the first time in Chrome. This prevents the ESP from changing out from
  // under a viewer.
  bool check_for_new_catalog_ = true;

  // Used to control access to the ExploreSitesStore.
  TaskQueue task_queue_;
  std::unique_ptr<ExploreSitesStore> explore_sites_store_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesServiceImpl);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_IMPL_H_
