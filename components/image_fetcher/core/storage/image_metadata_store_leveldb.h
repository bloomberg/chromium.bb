// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_STORAGE_IMAGE_METADATA_STORE_LEVELDB_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_STORAGE_IMAGE_METADATA_STORE_LEVELDB_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/image_fetcher/core/storage/image_metadata_store.h"
#include "components/image_fetcher/core/storage/image_store_types.h"
#include "components/leveldb_proto/proto_database.h"

namespace image_fetcher {

class CachedImageMetadataProto;

// Stores image metadata in leveldb.
class ImageMetadataStoreLevelDB : public ImageMetadataStore {
 public:
  // Initializes the database with |database_dir|.
  ImageMetadataStoreLevelDB(const base::FilePath& database_dir);

  // Initializes the database with |database_dir|. Creates storage using the
  // given |image_database| for local storage. Useful for testing.
  ImageMetadataStoreLevelDB(
      const base::FilePath& database_dir,
      std::unique_ptr<leveldb_proto::ProtoDatabase<CachedImageMetadataProto>>
          database);
  ~ImageMetadataStoreLevelDB() override;

  // ImageMetadataStorage:
  void Initialize(base::OnceClosure callback) override;
  bool IsInitialized() override;
  void SaveImageMetadata(const std::string& key,
                         const size_t data_size) override;
  void DeleteImageMetadata(const std::string& key) override;
  void CheckExistsAndUpdate(const std::string& key,
                            ImageStoreOperationCallback callback) override;
  void GetAllKeys(KeysCallback callback) override;
  void EvictImageMetadata(base::Time expiration_time,
                          const size_t bytes_left,
                          KeysCallback callback) override;

 private:
  InitializationStatus initialzation_status_;

  std::unique_ptr<leveldb_proto::ProtoDatabase<CachedImageMetadataProto>>
      database_;

  base::WeakPtrFactory<ImageMetadataStoreLevelDB> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageMetadataStoreLevelDB);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_STORAGE_IMAGE_METADATA_STORE_LEVELDB_H_
