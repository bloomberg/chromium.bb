// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_STORAGE_IMAGE_DATA_STORE_DISK_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_STORAGE_IMAGE_DATA_STORE_DISK_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/image_fetcher/core/storage/image_data_store.h"
#include "components/image_fetcher/core/storage/image_store_types.h"

namespace image_fetcher {

// Stores image data on disk.
class ImageDataStoreDisk : public ImageDataStore {
 public:
  // Stores the image data under the given |storage_path|.
  explicit ImageDataStoreDisk(const std::string& storage_path);
  ~ImageDataStoreDisk() override;

  // ImageDataStorage:
  void Initialize(base::OnceClosure callback) override;
  bool IsInitialized() override;
  void SaveImage(const std::string& key, const std::string& data) override;
  void LoadImage(const std::string& key, ImageDataCallback callback) override;
  void DeleteImage(const std::string& key) override;
  void GetAllKeys(KeysCallback callback) override;

 private:
  InitializationStatus initialzation_status_;

  // Path to where the image data will be stored.
  std::string storage_path_;

  base::WeakPtrFactory<ImageDataStoreDisk> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageDataStoreDisk);
};
}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_STORAGE_IMAGE_DATA_STORE_DISK_H_
