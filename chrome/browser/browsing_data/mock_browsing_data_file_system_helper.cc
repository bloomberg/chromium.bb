// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/browsing_data/mock_browsing_data_file_system_helper.h"

MockBrowsingDataFileSystemHelper::MockBrowsingDataFileSystemHelper(
    Profile* profile) {
}

MockBrowsingDataFileSystemHelper::~MockBrowsingDataFileSystemHelper() {
}

void MockBrowsingDataFileSystemHelper::StartFetching(
    const base::Callback<void(const std::list<FileSystemInfo>&)>& callback) {
  callback_ = callback;
}

void MockBrowsingDataFileSystemHelper::DeleteFileSystemOrigin(
    const GURL& origin) {
  std::string key = origin.spec();
  CHECK(file_systems_.find(key) != file_systems_.end());
  last_deleted_origin_ = origin;
  file_systems_[key] = false;
}

void MockBrowsingDataFileSystemHelper::AddFileSystem(
    const GURL& origin, bool has_persistent, bool has_temporary,
    bool has_syncable) {
  BrowsingDataFileSystemHelper::FileSystemInfo info(origin);
  if (has_persistent)
    info.usage_map[fileapi::kFileSystemTypePersistent] = 0;
  if (has_temporary)
    info.usage_map[fileapi::kFileSystemTypeTemporary] = 0;
  if (has_syncable)
    info.usage_map[fileapi::kFileSystemTypeSyncable] = 0;
  response_.push_back(info);
  file_systems_[origin.spec()] = true;
}

void MockBrowsingDataFileSystemHelper::AddFileSystemSamples() {
  AddFileSystem(GURL("http://fshost1:1/"), false, true, false);
  AddFileSystem(GURL("http://fshost2:2/"), true, false, true);
  AddFileSystem(GURL("http://fshost3:3/"), true, true, true);
}

void MockBrowsingDataFileSystemHelper::Notify() {
  CHECK_EQ(false, callback_.is_null());
  callback_.Run(response_);
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
