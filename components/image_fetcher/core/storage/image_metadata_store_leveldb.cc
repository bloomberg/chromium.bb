// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/storage/image_metadata_store_leveldb.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/image_fetcher/core/storage/proto/cached_image_metadata.pb.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace image_fetcher {

// TODO(wylieb): Implement method stubs.
ImageMetadataStoreLevelDB::ImageMetadataStoreLevelDB(
    const base::FilePath& database_dir)
    : ImageMetadataStoreLevelDB(database_dir, nullptr) {}

ImageMetadataStoreLevelDB::ImageMetadataStoreLevelDB(
    const base::FilePath& database_dir,
    std::unique_ptr<leveldb_proto::ProtoDatabase<CachedImageMetadataProto>>
        database)
    : initialzation_status_(InitializationStatus::UNINITIALIZED),
      database_(std::move(database)),
      weak_ptr_factory_(this) {}

ImageMetadataStoreLevelDB::~ImageMetadataStoreLevelDB() = default;

void ImageMetadataStoreLevelDB::Initialize(base::OnceClosure callback) {}

bool ImageMetadataStoreLevelDB::IsInitialized() {
  return initialzation_status_ == InitializationStatus::INITIALIZED ||
         initialzation_status_ == InitializationStatus::INIT_FAILURE;
}

void ImageMetadataStoreLevelDB::SaveImageMetadata(const std::string& key,
                                                  const size_t data_size) {}

void ImageMetadataStoreLevelDB::DeleteImageMetadata(const std::string& key) {}

void ImageMetadataStoreLevelDB::CheckExistsAndUpdate(
    const std::string& key,
    ImageStoreOperationCallback callback) {}

void ImageMetadataStoreLevelDB::GetAllKeys(KeysCallback callback) {}

void ImageMetadataStoreLevelDB::EvictImageMetadata(base::Time expiration_time,
                                                   const size_t bytes_left,
                                                   KeysCallback callback) {}

}  // namespace image_fetcher
