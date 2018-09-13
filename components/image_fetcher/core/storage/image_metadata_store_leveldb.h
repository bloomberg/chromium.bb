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

namespace base {
class Clock;
class SequencedTaskRunner;
}  // namespace base

namespace image_fetcher {

class CachedImageMetadataProto;

// Stores image metadata in leveldb.
class ImageMetadataStoreLevelDB : public ImageMetadataStore {
 public:
  // Initializes the database with |database_dir|.
  ImageMetadataStoreLevelDB(
      const base::FilePath& database_dir,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::Clock* clock);

  // Initializes the database with |database_dir|. Creates storage using the
  // given |image_database| for local storage. Useful for testing.
  ImageMetadataStoreLevelDB(
      const base::FilePath& database_dir,
      std::unique_ptr<leveldb_proto::ProtoDatabase<CachedImageMetadataProto>>
          database,
      base::Clock* clock);
  ~ImageMetadataStoreLevelDB() override;

  // ImageMetadataStorage:
  void Initialize(base::OnceClosure callback) override;
  bool IsInitialized() override;
  void SaveImageMetadata(const std::string& key,
                         const size_t data_size) override;
  void DeleteImageMetadata(const std::string& key) override;
  void UpdateImageMetadata(const std::string& key) override;
  void GetAllKeys(KeysCallback callback) override;
  void EvictImageMetadata(base::Time expiration_time,
                          const size_t bytes_left,
                          KeysCallback callback) override;

 private:
  void OnDatabaseInitialized(base::OnceClosure callback, bool success);
  void OnImageUpdated(bool success);
  void UpdateImageMetadataImpl(
      bool success,
      std::unique_ptr<std::vector<CachedImageMetadataProto>> entries);
  void GetAllKeysImpl(KeysCallback callback,
                      bool success,
                      std::unique_ptr<std::vector<std::string>> keys);
  void EvictImageMetadataImpl(
      base::Time expiration_time,
      const size_t bytes_left,
      KeysCallback callback,
      bool success,
      std::unique_ptr<std::vector<CachedImageMetadataProto>> entries);
  void OnEvictImageMetadataDone(KeysCallback callback,
                                std::vector<std::string> deleted_keys,
                                bool success);

  InitializationStatus initialization_status_;
  base::FilePath database_dir_;
  std::unique_ptr<leveldb_proto::ProtoDatabase<CachedImageMetadataProto>>
      database_;
  // Clock is owned by the service that creates this object.
  base::Clock* clock_;
  base::WeakPtrFactory<ImageMetadataStoreLevelDB> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageMetadataStoreLevelDB);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_STORAGE_IMAGE_METADATA_STORE_LEVELDB_H_
