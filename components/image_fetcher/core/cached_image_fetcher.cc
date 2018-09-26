// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cached_image_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/image_fetcher/core/storage/image_cache.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

namespace image_fetcher {

namespace {

// Wrapper to check if callbacks can be called.
void CallbackIfPresent(ImageDataFetcherCallback data_callback,
                       ImageFetcherCallback image_callback,
                       const std::string& id,
                       const std::string& image_data,
                       const gfx::Image& image,
                       const image_fetcher::RequestMetadata& metadata) {
  if (!data_callback.is_null())
    std::move(data_callback).Run(image_data, metadata);
  if (!image_callback.is_null())
    std::move(image_callback).Run(id, image, metadata);
}

}  // namespace

CachedImageFetcher::CachedImageFetcher(
    std::unique_ptr<ImageFetcher> image_fetcher,
    std::unique_ptr<ImageCache> image_cache)
    : image_fetcher_(std::move(image_fetcher)),
      image_cache_(std::move(image_cache)),
      weak_ptr_factory_(this) {
  DCHECK(image_fetcher_);
  DCHECK(image_cache_);
}

CachedImageFetcher::~CachedImageFetcher() = default;

void CachedImageFetcher::SetDataUseServiceName(
    DataUseServiceName data_use_service_name) {
  image_fetcher_->SetDataUseServiceName(data_use_service_name);
}

void CachedImageFetcher::SetDesiredImageFrameSize(const gfx::Size& size) {
  image_fetcher_->SetDesiredImageFrameSize(size);
}

void CachedImageFetcher::SetImageDownloadLimit(
    base::Optional<int64_t> max_download_bytes) {
  image_fetcher_->SetImageDownloadLimit(max_download_bytes);
}

ImageDecoder* CachedImageFetcher::GetImageDecoder() {
  return image_fetcher_->GetImageDecoder();
}

void CachedImageFetcher::FetchImageAndData(
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback data_callback,
    ImageFetcherCallback image_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  // First, try to load the image from the cache, then try the network.
  image_cache_->LoadImage(
      image_url.spec(),
      base::BindOnce(&CachedImageFetcher::OnImageFetchedFromCache,
                     weak_ptr_factory_.GetWeakPtr(), id, image_url,
                     std::move(data_callback), std::move(image_callback),
                     traffic_annotation));
}

void CachedImageFetcher::OnImageFetchedFromCache(
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback data_callback,
    ImageFetcherCallback image_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    std::string image_data) {
  if (image_data.empty()) {
    // Fetching from the DB failed, start a network fetch.
    FetchImageFromNetwork(id, image_url, std::move(data_callback),
                          std::move(image_callback), traffic_annotation);
  } else {
    GetImageDecoder()->DecodeImage(
        image_data, gfx::Size(),
        base::BindRepeating(&CachedImageFetcher::OnImageDecodedFromCache,
                            weak_ptr_factory_.GetWeakPtr(), id, image_url,
                            base::Passed(std::move(data_callback)),
                            base::Passed(std::move(image_callback)),
                            traffic_annotation, image_data));
  }
}

void CachedImageFetcher::OnImageDecodedFromCache(
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback data_callback,
    ImageFetcherCallback image_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& image_data,
    const gfx::Image& image) {
  if (image.IsEmpty()) {
    FetchImageFromNetwork(id, image_url, std::move(data_callback),
                          std::move(image_callback), traffic_annotation);
    image_cache_->DeleteImage(image_url.spec());
  } else {
    CallbackIfPresent(std::move(data_callback), std::move(image_callback), id,
                      image_data, image, RequestMetadata());
  }
}

void CachedImageFetcher::FetchImageFromNetwork(
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback data_callback,
    ImageFetcherCallback image_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  if (!image_url.is_valid()) {
    // URL is invalid, return empty image/data.
    CallbackIfPresent(std::move(data_callback), std::move(image_callback), id,
                      std::string(), gfx::Image(), RequestMetadata());
    return;
  }

  image_fetcher_->FetchImageData(
      id, image_url,
      base::BindOnce(&CachedImageFetcher::OnImageFetchedFromNetwork,
                     weak_ptr_factory_.GetWeakPtr(), id, image_url,
                     std::move(data_callback), std::move(image_callback)),
      traffic_annotation);
}

void CachedImageFetcher::OnImageFetchedFromNetwork(
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback data_callback,
    ImageFetcherCallback image_callback,
    const std::string& image_data,
    const RequestMetadata& request_metadata) {
  if (image_data.empty()) {
    // Fetching image failed, return empty image/data.
    CallbackIfPresent(std::move(data_callback), std::move(image_callback), id,
                      image_data, gfx::Image(), request_metadata);
    return;
  }

  image_fetcher_->GetImageDecoder()->DecodeImage(
      image_data, gfx::Size(),
      base::BindRepeating(&CachedImageFetcher::OnImageDecodedFromNetwork,
                          weak_ptr_factory_.GetWeakPtr(), id, image_url,
                          base::Passed(std::move(data_callback)),
                          base::Passed(std::move(image_callback)), image_data,
                          request_metadata));
}

void CachedImageFetcher::OnImageDecodedFromNetwork(
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback data_callback,
    ImageFetcherCallback image_callback,
    const std::string& image_data,
    const RequestMetadata& request_metadata,
    const gfx::Image& image) {
  CallbackIfPresent(std::move(data_callback), std::move(image_callback), id,
                    image_data, image, request_metadata);

  if (!image.IsEmpty()) {
    image_cache_->SaveImage(image_url.spec(), image_data);
  }
}

}  // namespace image_fetcher
