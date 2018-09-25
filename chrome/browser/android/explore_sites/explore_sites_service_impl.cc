// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_service_impl.h"

#include "base/task/post_task.h"
#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/get_catalog_task.h"
#include "chrome/browser/android/explore_sites/get_images_task.h"
#include "chrome/browser/android/explore_sites/import_catalog_task.h"
#include "components/offline_pages/task/task.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

using chrome::android::explore_sites::ExploreSitesVariation;
using chrome::android::explore_sites::GetExploreSitesVariation;

namespace explore_sites {
namespace {
const int kFaviconsPerCategoryImage = 4;
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
      base::BindOnce(&ExploreSitesServiceImpl::DecodeImageBytes,
                     std::move(callback))));
  // TODO(dewittj, freedjm): implement.
  std::move(callback).Run(nullptr);
}

void ExploreSitesServiceImpl::GetSiteImage(int site_id,
                                           BitmapCallback callback) {
  task_queue_.AddTask(std::make_unique<GetImagesTask>(
      explore_sites_store_.get(), site_id,
      base::BindOnce(&ExploreSitesServiceImpl::DecodeImageBytes,
                     std::move(callback))));
}

void ExploreSitesServiceImpl::Shutdown() {}

void ExploreSitesServiceImpl::OnTaskQueueIsIdle() {}

void ExploreSitesServiceImpl::AddUpdatedCatalog(
    std::string version_token,
    std::unique_ptr<Catalog> catalog_proto) {
  if (!IsExploreSitesEnabled())
    return;

  task_queue_.AddTask(std::make_unique<ImportCatalogTask>(
      explore_sites_store_.get(), version_token, std::move(catalog_proto)));
}

// static
void ExploreSitesServiceImpl::OnDecodeDone(BitmapCallback callback,
                                           const SkBitmap& decoded_image) {
  std::unique_ptr<SkBitmap> bitmap = std::make_unique<SkBitmap>(decoded_image);
  std::move(callback).Run(std::move(bitmap));
}

// static
void ExploreSitesServiceImpl::DecodeImageBytes(BitmapCallback callback,
                                               EncodedImageList images) {
  // TODO(freedjm) Fix to handle multiple images when support is added for
  // creating composite images for the NTP tiles.
  DCHECK(images.size() > 0);
  std::unique_ptr<EncodedImageBytes> image_data = std::move(images[0]);

  service_manager::mojom::ConnectorRequest connector_request;
  std::unique_ptr<service_manager::Connector> connector =
      service_manager::Connector::Create(&connector_request);
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindConnectorRequest(std::move(connector_request));

  data_decoder::DecodeImage(connector.get(), *image_data,
                            data_decoder::mojom::ImageCodec::DEFAULT, false,
                            data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
                            base::BindOnce(&OnDecodeDone, std::move(callback)));
}

}  // namespace explore_sites
