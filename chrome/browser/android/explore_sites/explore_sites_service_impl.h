// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_IMPL_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/android/explore_sites/explore_sites_fetcher.h"
#include "chrome/browser/android/explore_sites/explore_sites_service.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "chrome/browser/android/explore_sites/history_statistics_reporter.h"
#include "components/offline_pages/task/task_queue.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using offline_pages::TaskQueue;

namespace explore_sites {
class Catalog;

class ExploreSitesServiceImpl : public ExploreSitesService,
                                public TaskQueue::Delegate {
 public:
  ExploreSitesServiceImpl(
      std::unique_ptr<ExploreSitesStore> store,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<HistoryStatisticsReporter> history_statistics_reporter);
  ~ExploreSitesServiceImpl() override;

  static bool IsExploreSitesEnabled();

  // ExploreSitesService implementation.
  void GetCatalog(CatalogCallback callback) override;
  void GetCategoryImage(int category_id, BitmapCallback callback) override;
  void GetSiteImage(int site_id, BitmapCallback callback) override;
  void UpdateCatalogFromNetwork(std::string accept_languages,
                                BooleanCallback callback) override;

 private:
  // KeyedService implementation:
  void Shutdown() override;

  // TaskQueue::Delegate implementation:
  void OnTaskQueueIsIdle() override;

  void AddUpdatedCatalog(std::string version_token,
                         std::unique_ptr<Catalog> catalog_proto,
                         BooleanCallback callback);

  static void OnDecodeDone(BitmapCallback callback,
                           const SkBitmap& decoded_image);
  static void DecodeImageBytes(BitmapCallback callback,
                               EncodedImageList images);

  // Callback returning from the UpdateCatalogFromNetwork operation.  It
  // passes along the call back to the bridge and eventually back to Java land.
  void OnCatalogFetched(BooleanCallback callback,
                        ExploreSitesRequestStatus status,
                        std::unique_ptr<std::string> serialized_protobuf);

  // True when Chrome starts up, this is reset after the catalog is requested
  // the first time in Chrome. This prevents the ESP from changing out from
  // under a viewer.
  bool check_for_new_catalog_ = true;

  // Used to control access to the ExploreSitesStore.
  TaskQueue task_queue_;
  std::unique_ptr<ExploreSitesStore> explore_sites_store_;
  scoped_refptr<network ::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<ExploreSitesFetcher> explore_sites_fetcher_;
  std::unique_ptr<HistoryStatisticsReporter> history_statistics_reporter_;
  base::WeakPtrFactory<ExploreSitesServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesServiceImpl);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_IMPL_H_
