// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_service_impl.h"

#include "base/logging.h"
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
#include "services/network/public/cpp/shared_url_loader_factory.h"
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
    std::unique_ptr<ExploreSitesStore> store,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : task_queue_(this),
      explore_sites_store_(std::move(store)),
      url_loader_factory_(url_loader_factory),
      weak_ptr_factory_(this) {}

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

void ExploreSitesServiceImpl::UpdateCatalogFromNetwork(
    BooleanCallback callback) {
  if (!IsExploreSitesEnabled())
    return;
  // If we are already fetching, don't interrupt a fetch in progress.
  if (explore_sites_fetcher_ != nullptr)
    return;

  // TODO(petewil): Eventually get the catalog version from DB.
  std::string catalog_version = "";
  // TODO(petewil): Eventually get the country code from somewhere.
  std::string country_code = "KE";

  // Create a fetcher and start fetching the protobuf (async).
  explore_sites_fetcher_ = ExploreSitesFetcher::CreateForGetCatalog(
      base::BindOnce(&ExploreSitesServiceImpl::OnCatalogFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      catalog_version, country_code, url_loader_factory_);
}

void ExploreSitesServiceImpl::OnCatalogFetched(
    BooleanCallback callback,
    ExploreSitesRequestStatus status,
    std::unique_ptr<std::string> serialized_protobuf) {
  explore_sites_fetcher_.reset(nullptr);

  if (serialized_protobuf == nullptr) {
    DVLOG(1) << "Empty catalog response received from network.";
    std::move(callback).Run(false);
    return;
  }

  // Convert the protobuf into a catalog object.
  std::unique_ptr<explore_sites::GetCatalogResponse> catalog_response =
      std::make_unique<explore_sites::GetCatalogResponse>();
  if (!catalog_response->ParseFromString(*serialized_protobuf.get())) {
    DVLOG(1) << "Failed to parse catalog";
    std::move(callback).Run(false);
    return;
  }
  std::string catalog_version = catalog_response->version_token();
  std::unique_ptr<Catalog> catalog(catalog_response->release_catalog());

  // Add the catalog to our internal database using AddUpdatedCatalog
  AddUpdatedCatalog(catalog_version, std::move(catalog), std::move(callback));
}

void ExploreSitesServiceImpl::Shutdown() {}

void ExploreSitesServiceImpl::OnTaskQueueIsIdle() {}

void ExploreSitesServiceImpl::AddUpdatedCatalog(
    std::string version_token,
    std::unique_ptr<Catalog> catalog_proto,
    BooleanCallback callback) {
  if (!IsExploreSitesEnabled())
    return;

  task_queue_.AddTask(std::make_unique<ImportCatalogTask>(
      explore_sites_store_.get(), version_token, std::move(catalog_proto),
      std::move(callback)));
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
