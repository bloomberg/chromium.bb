// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index_on_disk.h"

#include "base/logging.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"

namespace sync_file_system {
namespace drive_backend {

MetadataDatabaseIndexOnDisk::MetadataDatabaseIndexOnDisk(leveldb::DB* db)
    : db_(db) {
  // TODO(peria): Add UMA to measure the number of FileMetadata, FileTracker,
  //    and AppRootId.
  NOTIMPLEMENTED();
}

MetadataDatabaseIndexOnDisk::~MetadataDatabaseIndexOnDisk() {}

const FileTracker* MetadataDatabaseIndexOnDisk::GetFileTracker(
    int64 tracker_id) const {
  NOTIMPLEMENTED();
  return NULL;
}

const FileMetadata* MetadataDatabaseIndexOnDisk::GetFileMetadata(
    const std::string& file_id) const {
  NOTIMPLEMENTED();
  return NULL;
}

void MetadataDatabaseIndexOnDisk::StoreFileMetadata(
    scoped_ptr<FileMetadata> metadata, leveldb::WriteBatch* batch) {
  PutFileMetadataToBatch(*metadata.get(), batch);
  NOTIMPLEMENTED();
}

void MetadataDatabaseIndexOnDisk::StoreFileTracker(
    scoped_ptr<FileTracker> tracker, leveldb::WriteBatch* batch) {
  PutFileTrackerToBatch(*tracker.get(), batch);
  NOTIMPLEMENTED();
}

void MetadataDatabaseIndexOnDisk::RemoveFileMetadata(
    const std::string& file_id, leveldb::WriteBatch* batch) {
  PutFileMetadataDeletionToBatch(file_id, batch);
  NOTIMPLEMENTED();
}

void MetadataDatabaseIndexOnDisk::RemoveFileTracker(
    int64 tracker_id, leveldb::WriteBatch* batch) {
  PutFileTrackerDeletionToBatch(tracker_id, batch);
  NOTIMPLEMENTED();
}

TrackerIDSet MetadataDatabaseIndexOnDisk::GetFileTrackerIDsByFileID(
    const std::string& file_id) const {
  NOTIMPLEMENTED();
  return TrackerIDSet();
}

int64 MetadataDatabaseIndexOnDisk::GetAppRootTracker(
    const std::string& app_id) const {
  NOTIMPLEMENTED();
  return 0;
}

TrackerIDSet MetadataDatabaseIndexOnDisk::GetFileTrackerIDsByParentAndTitle(
    int64 parent_tracker_id,
    const std::string& title) const {
  NOTIMPLEMENTED();
  return TrackerIDSet();
}

std::vector<int64> MetadataDatabaseIndexOnDisk::GetFileTrackerIDsByParent(
    int64 parent_tracker_id) const {
  NOTIMPLEMENTED();
  return std::vector<int64>();
}

std::string MetadataDatabaseIndexOnDisk::PickMultiTrackerFileID() const {
  NOTIMPLEMENTED();
  return std::string();
}

ParentIDAndTitle MetadataDatabaseIndexOnDisk::PickMultiBackingFilePath() const {
  NOTIMPLEMENTED();
  return ParentIDAndTitle();
}

int64 MetadataDatabaseIndexOnDisk::PickDirtyTracker() const {
  NOTIMPLEMENTED();
  return 0;
}

void MetadataDatabaseIndexOnDisk::DemoteDirtyTracker(int64 tracker_id) {
  NOTIMPLEMENTED();
}

bool MetadataDatabaseIndexOnDisk::HasDemotedDirtyTracker() const {
  NOTIMPLEMENTED();
  return true;
}

void MetadataDatabaseIndexOnDisk::PromoteDemotedDirtyTrackers() {
  NOTIMPLEMENTED();
}

size_t MetadataDatabaseIndexOnDisk::CountDirtyTracker() const {
  NOTIMPLEMENTED();
  return 0;
}

size_t MetadataDatabaseIndexOnDisk::CountFileMetadata() const {
  NOTIMPLEMENTED();
  return 0;
}

size_t MetadataDatabaseIndexOnDisk::CountFileTracker() const {
  NOTIMPLEMENTED();
  return 0;
}

std::vector<std::string>
MetadataDatabaseIndexOnDisk::GetRegisteredAppIDs() const {
  NOTIMPLEMENTED();
  return std::vector<std::string>();
}

std::vector<int64> MetadataDatabaseIndexOnDisk::GetAllTrackerIDs() const {
  NOTIMPLEMENTED();
  return std::vector<int64>();
}

std::vector<std::string>
MetadataDatabaseIndexOnDisk::GetAllMetadataIDs() const {
  NOTIMPLEMENTED();
  return std::vector<std::string>();
}

}  // namespace drive_backend
}  // namespace sync_file_system
