// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"

#include "chrome/browser/google_apis/drive_api_parser.h"

namespace sync_file_system {
namespace drive_backend {

MetadataDatabase::MetadataDatabase(base::SequencedTaskRunner* task_runner) {
  NOTIMPLEMENTED();
}

MetadataDatabase::~MetadataDatabase() {
}

void MetadataDatabase::Initialize(const base::FilePath& database_dir,
                                  const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

int64 MetadataDatabase::GetLargestChangeID() const {
  NOTIMPLEMENTED();
  return 0;
}

void MetadataDatabase::RegisterApp(const std::string& app_id,
                                   const std::string& folder_id,
                                   const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void MetadataDatabase::DisableApp(const std::string& app_id,
                                 const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void MetadataDatabase::EnableApp(const std::string& app_id,
                                 const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void MetadataDatabase::UnregisterApp(const std::string& app_id,
                                     const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

bool MetadataDatabase::FindAppRootFolder(const std::string& app_id,
                                         DriveFileMetadata* folder) const {
  NOTIMPLEMENTED();
  return false;
}

bool MetadataDatabase::FindFileByFileID(const std::string& file_id,
                                        DriveFileMetadata* metadata) const {
  NOTIMPLEMENTED();
  return false;
}

size_t MetadataDatabase::FindFilesByParentAndTitle(
    const std::string& file_id,
    const std::string& title,
    ScopedVector<DriveFileMetadata>* files) const {
  NOTIMPLEMENTED();
  return 0;
}

bool MetadataDatabase::FindActiveFileByParentAndTitle(
    const std::string& folder_id,
    const std::string& title,
    DriveFileMetadata* file) const {
  NOTIMPLEMENTED();
  return false;
}

bool MetadataDatabase::FindActiveFileByPath(const std::string& app_id,
                                            const base::FilePath& path,
                                            DriveFileMetadata* file) const {
  NOTIMPLEMENTED();
  return false;
}

bool MetadataDatabase::ConstructPathForFile(const std::string& file_id,
                                            base::FilePath* path) const {
  NOTIMPLEMENTED();
  return false;
}

void MetadataDatabase::UpdateByChangeList(
    ScopedVector<google_apis::ChangeResource> changes,
    const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void MetadataDatabase::PopulateFolder(
    const std::string& folder_id,
    ScopedVector<google_apis::ResourceEntry> children,
    const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

}  // namespace drive_backend
}  // namespace sync_file_system
