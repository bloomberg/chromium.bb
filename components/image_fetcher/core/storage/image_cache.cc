// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/storage/image_cache.h"

#include <utility>

#include "base/time/time.h"
#include "components/image_fetcher/core/storage/image_data_store.h"
#include "components/image_fetcher/core/storage/image_metadata_store.h"

namespace image_fetcher {

// TODO(wylieb): Implement method stubs.
// TODO(wylieb): Queue requests made before storage is ready.
ImageCache::ImageCache(std::unique_ptr<ImageDataStore> data_store,
                       std::unique_ptr<ImageMetadataStore> metadata_store)
    : data_store_(std::move(data_store)),
      metadata_store_(std::move(metadata_store)),
      weak_ptr_factory_(this) {}

ImageCache::~ImageCache() = default;

void ImageCache::SaveImage(const std::string& url,
                           const std::string& image_data) {}

void ImageCache::LoadImage(const std::string& url, ImageDataCallback callback) {
}

void ImageCache::DeleteImage(const std::string& url) {}

}  // namespace image_fetcher
