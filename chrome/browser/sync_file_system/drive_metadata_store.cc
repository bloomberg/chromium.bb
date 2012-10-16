// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_metadata_store.h"

#include "base/callback.h"
#include "base/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "googleurl/src/gurl.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "webkit/fileapi/file_system_url.h"

namespace sync_file_system {

class DriveMetadataDB : public base::NonThreadSafe {
 public:
  explicit DriveMetadataDB(const FilePath& base_dir);
  ~DriveMetadataDB();

 private:
  scoped_ptr<leveldb::DB> db;

  DISALLOW_COPY_AND_ASSIGN(DriveMetadataDB);
};

DriveMetadataStore::DriveMetadataStore(
    const FilePath& db_dir,
    base::SequencedTaskRunner* file_task_runner)
  : file_task_runner_(file_task_runner),
    db_disabled_(true),
    largest_changestamp_(0) {
  DCHECK(file_task_runner);
}

DriveMetadataStore::~DriveMetadataStore() {
  DCHECK(CalledOnValidThread());

  if (db_.get())
    file_task_runner_->DeleteSoon(FROM_HERE, db_.release());
}

void DriveMetadataStore::InitializeFromDiskCache(
    const InitializationCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(false, false);
}

void DriveMetadataStore::InitializeWithDocumentFeed(
    scoped_ptr<gdata::DocumentFeed> feed,
    const InitializationCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(false, false);
}

void DriveMetadataStore::SetLargestChangeStamp(int64 largest_changestamp) {
  NOTIMPLEMENTED();
  // TODO(tzik): Post task to FILE thread to update database entry.
  largest_changestamp_ = largest_changestamp;
}

int64 DriveMetadataStore::GetLargestChangeStamp() const {
  return largest_changestamp_;
}

void DriveMetadataStore::ApplyChangeFeed(
    scoped_ptr<gdata::DocumentFeed> feed) {
  NOTIMPLEMENTED();
}

std::string DriveMetadataStore::GetResourceId(
    const fileapi::FileSystemURL& url) const {
  NOTIMPLEMENTED();
  return std::string();
}

std::string DriveMetadataStore::GetETag(
    const fileapi::FileSystemURL& url) const {
  NOTIMPLEMENTED();
  return std::string();
}

GURL DriveMetadataStore::GetDownloadURL(
    const fileapi::FileSystemURL& url) const {
  NOTIMPLEMENTED();
  return GURL();
}

GURL DriveMetadataStore::GetUploadURL(
    const fileapi::FileSystemURL& url) const {
  NOTIMPLEMENTED();
  return GURL();
}

////////////////////////////////////////////////////////////////////////////////

DriveMetadataDB::DriveMetadataDB(const FilePath& base_dir) {
  NOTIMPLEMENTED();
}

DriveMetadataDB::~DriveMetadataDB() {
  DCHECK(CalledOnValidThread());
}

}  // namespace sync_file_system
