// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/mock_browsing_data_file_system_helper.h"

MockBrowsingDataFileSystemHelper::MockBrowsingDataFileSystemHelper(
    Profile* profile)
    : profile_(profile) {
}

MockBrowsingDataFileSystemHelper::~MockBrowsingDataFileSystemHelper() {
}

void MockBrowsingDataFileSystemHelper::StartFetching(
    Callback1<const std::vector<FileSystemInfo>& >::Type* callback) {
  callback_.reset(callback);
}

void MockBrowsingDataFileSystemHelper::CancelNotification() {
  callback_.reset(NULL);
}

void MockBrowsingDataFileSystemHelper::DeleteFileSystemOrigin(
    const GURL& origin) {
  std::string key = origin.spec();
  CHECK(file_systems_.find(key) != file_systems_.end());
  last_deleted_origin_ = origin;
  file_systems_[key] = false;
}

void MockBrowsingDataFileSystemHelper::AddFileSystem(
    const GURL& origin, bool has_persistent, bool has_temporary) {
  response_.push_back(BrowsingDataFileSystemHelper::FileSystemInfo(
      origin, has_persistent, has_temporary, 0, 0));
  file_systems_[origin.spec()] = true;
}

void MockBrowsingDataFileSystemHelper::AddFileSystemSamples() {
  AddFileSystem(GURL("http://fshost1:1/"), false, true);
  AddFileSystem(GURL("http://fshost2:2/"), true, false);
  AddFileSystem(GURL("http://fshost3:3/"), true, true);
}

void MockBrowsingDataFileSystemHelper::Notify() {
  CHECK(callback_.get());
  callback_->Run(response_);
}

void MockBrowsingDataFileSystemHelper::Reset() {
  for (std::map<const std::string, bool>::iterator i = file_systems_.begin();
       i != file_systems_.end(); ++i)
    i->second = true;
}

bool MockBrowsingDataFileSystemHelper::AllDeleted() {
  for (std::map<const std::string, bool>::const_iterator i =
            file_systems_.begin();
       i != file_systems_.end(); ++i) {
    if (i->second)
      return false;
  }
  return true;
}
