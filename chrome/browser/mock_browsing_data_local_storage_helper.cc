// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mock_browsing_data_local_storage_helper.h"

MockBrowsingDataLocalStorageHelper::MockBrowsingDataLocalStorageHelper(
  Profile* profile)
  : BrowsingDataLocalStorageHelper(profile),
    profile_(profile),
    delete_all_files_called_(false) {
}

MockBrowsingDataLocalStorageHelper::~MockBrowsingDataLocalStorageHelper() {
}

void MockBrowsingDataLocalStorageHelper::StartFetching(
    Callback1<const std::vector<LocalStorageInfo>& >::Type* callback) {
  callback_.reset(callback);
}

void MockBrowsingDataLocalStorageHelper::CancelNotification() {
  callback_.reset(NULL);
}

void MockBrowsingDataLocalStorageHelper::DeleteLocalStorageFile(
    const FilePath& file_path) {
  last_deleted_file_ = file_path;
}

void MockBrowsingDataLocalStorageHelper::DeleteAllLocalStorageFiles() {
  delete_all_files_called_ = true;
}

void MockBrowsingDataLocalStorageHelper::AddLocalStorageSamples() {
  response_.push_back(
      BrowsingDataLocalStorageHelper::LocalStorageInfo(
          "http", "host1", 1, "db1", "origin1",
          FilePath(FILE_PATH_LITERAL("file1")), 1, base::Time()));
  response_.push_back(
      BrowsingDataLocalStorageHelper::LocalStorageInfo(
          "http", "host2", 2, "db2", "origin2",
          FilePath(FILE_PATH_LITERAL("file2")), 2, base::Time()));
}

void MockBrowsingDataLocalStorageHelper::Notify() {
  CHECK(callback_.get());
  callback_->Run(response_);
}
