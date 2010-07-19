// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mock_browsing_data_local_storage_helper.h"

#include "base/callback.h"
#include "base/logging.h"

MockBrowsingDataLocalStorageHelper::MockBrowsingDataLocalStorageHelper(
  Profile* profile)
  : BrowsingDataLocalStorageHelper(profile),
    profile_(profile) {
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
  CHECK(files_.find(file_path.value()) != files_.end());
  last_deleted_file_ = file_path;
  files_[file_path.value()] = false;
}

void MockBrowsingDataLocalStorageHelper::AddLocalStorageSamples() {
  response_.push_back(
      BrowsingDataLocalStorageHelper::LocalStorageInfo(
          "http", "host1", 1, "db1", "http://host1:1/",
          FilePath(FILE_PATH_LITERAL("file1")), 1, base::Time()));
  files_[FILE_PATH_LITERAL("file1")] = true;
  response_.push_back(
      BrowsingDataLocalStorageHelper::LocalStorageInfo(
          "http", "host2", 2, "db2", "http://host2:2/",
          FilePath(FILE_PATH_LITERAL("file2")), 2, base::Time()));
  files_[FILE_PATH_LITERAL("file2")] = true;
}

void MockBrowsingDataLocalStorageHelper::Notify() {
  CHECK(callback_.get());
  callback_->Run(response_);
}

void MockBrowsingDataLocalStorageHelper::Reset() {
  for (std::map<const FilePath::StringType, bool>::iterator i = files_.begin();
       i != files_.end(); ++i)
    i->second = true;
}

bool MockBrowsingDataLocalStorageHelper::AllDeleted() {
  for (std::map<const FilePath::StringType, bool>::const_iterator i =
       files_.begin(); i != files_.end(); ++i)
    if (i->second)
      return false;
  return true;
}
