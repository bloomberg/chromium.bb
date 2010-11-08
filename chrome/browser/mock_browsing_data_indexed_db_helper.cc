// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mock_browsing_data_indexed_db_helper.h"

#include "base/callback.h"
#include "base/logging.h"

MockBrowsingDataIndexedDBHelper::MockBrowsingDataIndexedDBHelper(
    Profile* profile)
    : profile_(profile) {
}

MockBrowsingDataIndexedDBHelper::~MockBrowsingDataIndexedDBHelper() {
}

void MockBrowsingDataIndexedDBHelper::StartFetching(
    Callback1<const std::vector<IndexedDBInfo>& >::Type* callback) {
  callback_.reset(callback);
}

void MockBrowsingDataIndexedDBHelper::CancelNotification() {
  callback_.reset(NULL);
}

void MockBrowsingDataIndexedDBHelper::DeleteIndexedDBFile(
    const FilePath& file_path) {
  CHECK(files_.find(file_path.value()) != files_.end());
  last_deleted_file_ = file_path;
  files_[file_path.value()] = false;
}

void MockBrowsingDataIndexedDBHelper::AddIndexedDBSamples() {
  response_.push_back(
      BrowsingDataIndexedDBHelper::IndexedDBInfo(
          "http", "idbhost1", 1, "idb1", "http://idbhost1:1/",
          FilePath(FILE_PATH_LITERAL("file1")), 1, base::Time()));
  files_[FILE_PATH_LITERAL("file1")] = true;
  response_.push_back(
      BrowsingDataIndexedDBHelper::IndexedDBInfo(
          "http", "idbhost2", 2, "idb2", "http://idbhost2:2/",
          FilePath(FILE_PATH_LITERAL("file2")), 2, base::Time()));
  files_[FILE_PATH_LITERAL("file2")] = true;
}

void MockBrowsingDataIndexedDBHelper::Notify() {
  CHECK(callback_.get());
  callback_->Run(response_);
}

void MockBrowsingDataIndexedDBHelper::Reset() {
  for (std::map<const FilePath::StringType, bool>::iterator i = files_.begin();
       i != files_.end(); ++i)
    i->second = true;
}

bool MockBrowsingDataIndexedDBHelper::AllDeleted() {
  for (std::map<const FilePath::StringType, bool>::const_iterator i =
       files_.begin(); i != files_.end(); ++i)
    if (i->second)
      return false;
  return true;
}
