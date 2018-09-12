// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_STORAGE_IMAGE_CACHE_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_STORAGE_IMAGE_CACHE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/image_fetcher/core/storage/image_store_types.h"

namespace image_fetcher {

class ImageDataStore;
class ImageMetadataStore;

// Persist image meta/data via the given implementations of ImageDataStore and
// ImageMetadataStore.
class ImageCache {
  ImageCache(std::unique_ptr<ImageDataStore> data_storage,
             std::unique_ptr<ImageMetadataStore> metadata_storage);
  ~ImageCache();

  // Adds or updates the image data for the |url|. If the class hasn't been
  // initialized yet, the call is queued.
  void SaveImage(const std::string& url, const std::string& image_data);

  // Loads the image data for the |url| and passes it to |callback|.
  void LoadImage(const std::string& url, ImageDataCallback callback);

  // Deletes the image data for the |url|.
  void DeleteImage(const std::string& url);

 private:
  std::unique_ptr<ImageDataStore> data_store_;
  std::unique_ptr<ImageMetadataStore> metadata_store_;

  base::WeakPtrFactory<ImageCache> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageCache);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_STORAGE_IMAGE_CACHE_H_
