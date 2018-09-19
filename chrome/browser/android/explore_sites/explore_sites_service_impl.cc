// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_service_impl.h"

#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/get_catalog_task.h"
#include "chrome/browser/android/explore_sites/get_images_task.h"
#include "chrome/browser/android/explore_sites/import_catalog_task.h"
#include "components/offline_pages/task/task.h"

using chrome::android::explore_sites::ExploreSitesVariation;
using chrome::android::explore_sites::GetExploreSitesVariation;

namespace explore_sites {
namespace {
const int kFaviconsPerCategoryImage = 4;
}

void DecodeImageBytes(BitmapCallback callback, EncodedImageList images) {
  // TODO(freedjm): implement image decode.
  NOTIMPLEMENTED();
  std::move(callback).Run(nullptr);
}

ExploreSitesServiceImpl::ExploreSitesServiceImpl(
    std::unique_ptr<ExploreSitesStore> store)
    : task_queue_(this), explore_sites_store_(std::move(store)) {}

ExploreSitesServiceImpl::~ExploreSitesServiceImpl() {}

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
                                               BitmapCallback callback) {
  task_queue_.AddTask(std::make_unique<GetImagesTask>(
      explore_sites_store_.get(), category_id, kFaviconsPerCategoryImage,
      base::BindOnce(&DecodeImageBytes, std::move(callback))));
  // TODO(dewittj, freedjm): implement.
  std::move(callback).Run(nullptr);
}

void ExploreSitesServiceImpl::GetSiteImage(int site_id,
                                           BitmapCallback callback) {
  task_queue_.AddTask(std::make_unique<GetImagesTask>(
      explore_sites_store_.get(), site_id,
      base::BindOnce(&DecodeImageBytes, std::move(callback))));
}

void ExploreSitesServiceImpl::Shutdown() {}

void ExploreSitesServiceImpl::OnTaskQueueIsIdle() {}

void ExploreSitesServiceImpl::AddUpdatedCatalog(
    int64_t catalog_timestamp,
    std::unique_ptr<Catalog> catalog_proto) {
  if (!IsExploreSitesEnabled())
    return;

  task_queue_.AddTask(std::make_unique<ImportCatalogTask>(
      explore_sites_store_.get(), catalog_timestamp, std::move(catalog_proto)));
}

}  // namespace explore_sites
