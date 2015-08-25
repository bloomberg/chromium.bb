// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_file_system_helper.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

MockBrowsingDataFileSystemHelper::MockBrowsingDataFileSystemHelper(
    Profile* profile) {
}

MockBrowsingDataFileSystemHelper::~MockBrowsingDataFileSystemHelper() {
}

void MockBrowsingDataFileSystemHelper::StartFetching(
    const FetchCallback& callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = callback;
}

void MockBrowsingDataFileSystemHelper::DeleteFileSystemOrigin(
    const GURL& origin) {
  ASSERT_FALSE(callback_.is_null());
  std::string key = origin.spec();
  ASSERT_TRUE(ContainsKey(file_systems_, key));
  last_deleted_origin_ = origin;
  file_systems_[key] = false;
}

void MockBrowsingDataFileSystemHelper::AddFileSystem(
    const GURL& origin, bool has_persistent, bool has_temporary,
    bool has_syncable) {
  BrowsingDataFileSystemHelper::FileSystemInfo info(origin);
  if (has_persistent)
    info.usage_map[storage::kFileSystemTypePersistent] = 0;
  if (has_temporary)
    info.usage_map[storage::kFileSystemTypeTemporary] = 0;
  if (has_syncable)
    info.usage_map[storage::kFileSystemTypeSyncable] = 0;
  response_.push_back(info);
  file_systems_[origin.spec()] = true;
}

void MockBrowsingDataFileSystemHelper::AddFileSystemSamples() {
  AddFileSystem(GURL("http://fshost1:1/"), false, true, false);
  AddFileSystem(GURL("http://fshost2:2/"), true, false, true);
  AddFileSystem(GURL("http://fshost3:3/"), true, true, true);
}

void MockBrowsingDataFileSystemHelper::Notify() {
  callback_.Run(response_);
}

void MockBrowsingDataFileSystemHelper::Reset() {
  for (auto& pair : file_systems_)
    pair.second = true;
}

bool MockBrowsingDataFileSystemHelper::AllDeleted() {
  for (const auto& pair : file_systems_) {
    if (pair.second)
      return false;
  }
  return true;
}
