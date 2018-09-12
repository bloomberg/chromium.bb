// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/storage/image_data_store_disk.h"

namespace image_fetcher {

// TODO(wylieb): Implement method stubs.
ImageDataStoreDisk::ImageDataStoreDisk(const std::string& storage_path)
    : initialzation_status_(InitializationStatus::UNINITIALIZED),
      storage_path_(storage_path),
      weak_ptr_factory_(this) {}

ImageDataStoreDisk::~ImageDataStoreDisk() = default;

void ImageDataStoreDisk::Initialize(base::OnceClosure callback) {}

bool ImageDataStoreDisk::IsInitialized() {
  return initialzation_status_ == InitializationStatus::INITIALIZED ||
         initialzation_status_ == InitializationStatus::INIT_FAILURE;
}

void ImageDataStoreDisk::SaveImage(const std::string& key,
                                   const std::string& image_data) {}

void ImageDataStoreDisk::LoadImage(const std::string& key,
                                   ImageDataCallback callback) {}

void ImageDataStoreDisk::DeleteImage(const std::string& key) {}

void ImageDataStoreDisk::GetAllKeys(KeysCallback callback) {}

}  // namespace image_fetcher
