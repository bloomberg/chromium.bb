// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_request_handler.h"

#include <utility>

#include "base/bind.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"

namespace favicon {

namespace {

// Parameter used for local bitmap queries by page url. The url is an origin,
// and it may not have had a favicon associated with it. A trickier case is when
// it only has domain-scoped cookies, but visitors are redirected to HTTPS on
// visiting. It defaults to a HTTP scheme, but the favicon will be associated
// with the HTTPS URL and hence won't be found if we include the scheme in the
// lookup. Set |fallback_to_host|=true so the favicon database will fall back to
// matching only the hostname to have the best chance of finding a favicon.
// TODO(victorvianna): Consider passing this as a parameter in the API.
const bool kFallbackToHost = true;

// Parameter used for local bitmap queries by page url.
favicon_base::IconTypeSet GetIconTypesForLocalQuery() {
  return favicon_base::IconTypeSet{favicon_base::IconType::kFavicon};
}

}  // namespace

FaviconRequestHandler::FaviconRequestHandler() {}

FaviconRequestHandler::~FaviconRequestHandler() {}

void FaviconRequestHandler::GetRawFaviconForPageURL(
    const GURL& page_url,
    int desired_size_in_pixel,
    favicon_base::FaviconRawBitmapCallback callback,
    FaviconRequestOrigin request_origin,
    FaviconService* favicon_service,
    FaviconRequestHandler::SyncedFaviconGetter synced_favicon_getter,
    base::CancelableTaskTracker* tracker) {
  if (!favicon_service) {
    RecordFaviconRequestMetric(request_origin,
                               FaviconAvailability::kNotAvailable);
    std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
    return;
  }

  // First attempt to find the icon locally.
  favicon_service->GetRawFaviconForPageURL(
      page_url, GetIconTypesForLocalQuery(), desired_size_in_pixel,
      kFallbackToHost,
      base::BindOnce(&FaviconRequestHandler::OnBitmapLocalDataAvailable,
                     weak_ptr_factory_.GetWeakPtr(), page_url,
                     /*response_callback=*/std::move(callback), request_origin,
                     std::move(synced_favicon_getter)),
      tracker);
}

void FaviconRequestHandler::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::FaviconImageCallback callback,
    FaviconRequestOrigin request_origin,
    FaviconService* favicon_service,
    FaviconRequestHandler::SyncedFaviconGetter synced_favicon_getter,
    base::CancelableTaskTracker* tracker) {
  if (!favicon_service) {
    RecordFaviconRequestMetric(request_origin,
                               FaviconAvailability::kNotAvailable);
    std::move(callback).Run(favicon_base::FaviconImageResult());
    return;
  }

  // First attempt to find the icon locally.
  favicon_service->GetFaviconImageForPageURL(
      page_url,
      base::BindOnce(&FaviconRequestHandler::OnImageLocalDataAvailable,
                     weak_ptr_factory_.GetWeakPtr(), page_url,
                     /*response_callback=*/std::move(callback), request_origin,
                     std::move(synced_favicon_getter)),
      tracker);
}

void FaviconRequestHandler::OnBitmapLocalDataAvailable(
    const GURL& page_url,
    favicon_base::FaviconRawBitmapCallback response_callback,
    FaviconRequestOrigin origin,
    FaviconRequestHandler::SyncedFaviconGetter synced_favicon_getter,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) const {
  if (bitmap_result.is_valid()) {
    RecordFaviconRequestMetric(origin, FaviconAvailability::kLocal);
    std::move(response_callback).Run(bitmap_result);
    return;
  }

  scoped_refptr<base::RefCountedMemory> sync_bitmap;
  if (std::move(synced_favicon_getter).Run(page_url, &sync_bitmap)) {
    RecordFaviconRequestMetric(origin, FaviconAvailability::kSync);
    favicon_base::FaviconRawBitmapResult sync_bitmap_result;
    sync_bitmap_result.bitmap_data = sync_bitmap;
    std::move(response_callback).Run(sync_bitmap_result);
    return;
  }

  // If sync does not have the favicon, send empty response.
  RecordFaviconRequestMetric(origin, FaviconAvailability::kNotAvailable);
  std::move(response_callback).Run(favicon_base::FaviconRawBitmapResult());
}

void FaviconRequestHandler::OnImageLocalDataAvailable(
    const GURL& page_url,
    favicon_base::FaviconImageCallback response_callback,
    FaviconRequestOrigin origin,
    FaviconRequestHandler::SyncedFaviconGetter synced_favicon_getter,
    const favicon_base::FaviconImageResult& image_result) const {
  if (!image_result.image.IsEmpty()) {
    RecordFaviconRequestMetric(origin, FaviconAvailability::kLocal);
    std::move(response_callback).Run(image_result);
    return;
  }

  scoped_refptr<base::RefCountedMemory> sync_bitmap;
  if (std::move(synced_favicon_getter).Run(page_url, &sync_bitmap)) {
    RecordFaviconRequestMetric(origin, FaviconAvailability::kSync);
    favicon_base::FaviconImageResult sync_image_result;
    // Convert bitmap to image.
    sync_image_result.image =
        gfx::Image::CreateFrom1xPNGBytes(sync_bitmap.get());
    std::move(response_callback).Run(sync_image_result);
    return;
  }

  // If sync does not have the favicon, send empty response.
  RecordFaviconRequestMetric(origin, FaviconAvailability::kNotAvailable);
  std::move(response_callback).Run(favicon_base::FaviconImageResult());
}

}  // namespace favicon
