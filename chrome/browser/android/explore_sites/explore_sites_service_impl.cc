// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_service_impl.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chrome/browser/android/explore_sites/blacklist_site_task.h"
#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "chrome/browser/android/explore_sites/explore_sites_bridge.h"
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/get_catalog_task.h"
#include "chrome/browser/android/explore_sites/get_images_task.h"
#include "chrome/browser/android/explore_sites/image_helper.h"
#include "chrome/browser/android/explore_sites/import_catalog_task.h"
#include "components/offline_pages/task/task.h"
#include "content/public/browser/browser_thread.h"

using chrome::android::explore_sites::ExploreSitesVariation;
using chrome::android::explore_sites::GetExploreSitesVariation;

namespace explore_sites {

ExploreSitesServiceImpl::ExploreSitesServiceImpl(
    std::unique_ptr<ExploreSitesStore> store,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<HistoryStatisticsReporter> history_statistics_reporter)
    : task_queue_(this),
      explore_sites_store_(std::move(store)),
      url_loader_factory_(url_loader_factory),
      history_statistics_reporter_(std::move(history_statistics_reporter)),
      weak_ptr_factory_(this) {
  if (IsExploreSitesEnabled()) {
    ExploreSitesBridge::ScheduleDailyTask();
  }
  // Collect history statistics unconditionally, to have baseline as well.
  history_statistics_reporter_->ScheduleReportStatistics();
}

ExploreSitesServiceImpl::~ExploreSitesServiceImpl() {}

// static
bool ExploreSitesServiceImpl::IsExploreSitesEnabled() {
  return GetExploreSitesVariation() == ExploreSitesVariation::ENABLED;
}

void ExploreSitesServiceImpl::GetCatalog(CatalogCallback callback) {
  if (!IsExploreSitesEnabled())
    return;

  task_queue_.AddTask(std::make_unique<GetCatalogTask>(
      explore_sites_store_.get(), check_for_new_catalog_, std::move(callback)));
  check_for_new_catalog_ = false;
}

void ExploreSitesServiceImpl::GetCategoryImage(int category_id,
                                               int pixel_size,
                                               BitmapCallback callback) {
  task_queue_.AddTask(std::make_unique<GetImagesTask>(
      explore_sites_store_.get(), category_id, kFaviconsPerCategoryImage,
      base::BindOnce(&ExploreSitesServiceImpl::ComposeCategoryImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     pixel_size)));
}

void ExploreSitesServiceImpl::GetSiteImage(int site_id,
                                           BitmapCallback callback) {
  task_queue_.AddTask(std::make_unique<GetImagesTask>(
      explore_sites_store_.get(), site_id,
      base::BindOnce(&ExploreSitesServiceImpl::ComposeSiteImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void ExploreSitesServiceImpl::UpdateCatalogFromNetwork(
    bool is_immediate_fetch,
    const std::string& accept_languages,
    BooleanCallback callback) {
  if (!IsExploreSitesEnabled())
    return;

  update_catalog_callbacks_.push_back(std::move(callback));

  // If we are already fetching, don't interrupt a fetch in progress unless
  // immediate fetch is requested while currently running fetch is background
  // fetch. This is because we want to get back the result asap for immediate
  // fetch.
  if (explore_sites_fetcher_ != nullptr) {
    if (is_immediate_fetch)
      explore_sites_fetcher_->RestartAsImmediateFetchIfNotYet();
    return;
  }

  // TODO(petewil): Eventually get the catalog version from DB.
  std::string catalog_version = "";

  // Create a fetcher and start fetching the protobuf (async).
  explore_sites_fetcher_ = ExploreSitesFetcher::CreateForGetCatalog(
      is_immediate_fetch, catalog_version, accept_languages,
      url_loader_factory_,
      base::BindOnce(&ExploreSitesServiceImpl::OnCatalogFetched,
                     weak_ptr_factory_.GetWeakPtr()));
  explore_sites_fetcher_->Start();
}

void ExploreSitesServiceImpl::BlacklistSite(const std::string& url) {
  // Add the url to the blacklist table in the database.
  task_queue_.AddTask(
      std::make_unique<BlacklistSiteTask>(explore_sites_store_.get(), url));

  // TODO(https://crbug.com/893845): Remove cached category icon if affected.
}

// Validate the catalog.  Note this does not take ownership of the pointer.
void ValidateCatalog(Catalog* catalog) {
  // TODO(https://crbug.com/895541): Validate that the category type is in the
  // valid range.  Also make sure the site has a title, else remove it.

  // Convert the URLs to a standard format.
  for (auto& category : *catalog->mutable_categories()) {
    for (auto& site : *category.mutable_sites()) {
      site.set_site_url(GURL(site.site_url()).spec());
    }
  }
}

void ExploreSitesServiceImpl::OnCatalogFetched(
    ExploreSitesRequestStatus status,
    std::unique_ptr<std::string> serialized_protobuf) {
  UMA_HISTOGRAM_ENUMERATION("ExploreSites.RequestStatus", status);

  explore_sites_fetcher_.reset(nullptr);

  if (serialized_protobuf == nullptr) {
    DVLOG(1) << "Empty catalog response received from network.";
    NotifyCatalogUpdated(std::move(update_catalog_callbacks_), false);
    update_catalog_callbacks_.clear();
    return;
  }

  // Convert the protobuf into a catalog object.
  std::unique_ptr<explore_sites::GetCatalogResponse> catalog_response =
      std::make_unique<explore_sites::GetCatalogResponse>();
  if (!catalog_response->ParseFromString(*serialized_protobuf.get())) {
    DVLOG(1) << "Failed to parse catalog";
    NotifyCatalogUpdated(std::move(update_catalog_callbacks_), false);
    update_catalog_callbacks_.clear();
    return;
  }
  std::string catalog_version = catalog_response->version_token();
  std::unique_ptr<Catalog> catalog(catalog_response->release_catalog());

  // Check the catalog, canonicalizing any URLs in it.
  ValidateCatalog(catalog.get());

  // Add the catalog to our internal database.
  task_queue_.AddTask(std::make_unique<ImportCatalogTask>(
      explore_sites_store_.get(), catalog_version, std::move(catalog),
      base::BindOnce(&ExploreSitesServiceImpl::NotifyCatalogUpdated,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(update_catalog_callbacks_))));
  update_catalog_callbacks_.clear();
}

void ExploreSitesServiceImpl::ComposeSiteImage(BitmapCallback callback,
                                               EncodedImageList images) {
  image_helper_.ComposeSiteImage(std::move(callback), std::move(images));
}

void ExploreSitesServiceImpl::ComposeCategoryImage(BitmapCallback callback,
                                                   int pixel_size,
                                                   EncodedImageList images) {
  image_helper_.ComposeCategoryImage(std::move(callback), pixel_size,
                                     std::move(images));
}

void ExploreSitesServiceImpl::Shutdown() {}

void ExploreSitesServiceImpl::OnTaskQueueIsIdle() {}

void ExploreSitesServiceImpl::NotifyCatalogUpdated(
    std::vector<BooleanCallback> callbacks,
    bool success) {
  for (auto& callback : callbacks)
    std::move(callback).Run(success);
}
}  // namespace explore_sites
