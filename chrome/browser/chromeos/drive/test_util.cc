// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/test_util.h"

#include <string>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"

namespace drive {

namespace test_util {

TestCacheResource::TestCacheResource(const std::string& source_file,
                                     const std::string& resource_id,
                                     const std::string& md5,
                                     bool is_pinned,
                                     bool is_dirty) :
    source_file(source_file),
    resource_id(resource_id),
    md5(md5),
    is_pinned(is_pinned),
    is_dirty(is_dirty) {
}

std::vector<TestCacheResource> GetDefaultTestCacheResources() {
  const TestCacheResource resources[] = {
    // Cache resource in tmp dir, i.e. not pinned or dirty.
    TestCacheResource("cache.txt",
                      "tmp:resource_id",
                      "md5_tmp_alphanumeric",
                      false,
                      false),
    // Cache resource in tmp dir, i.e. not pinned or dirty, with resource_id
    // containing non-alphanumeric characters.
    TestCacheResource("cache2.png",
                      "tmp:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?",
                      "md5_tmp_non_alphanumeric",
                      false,
                      false),
    // Cache resource that is pinned and present.
    TestCacheResource("pinned/cache.mp3",
                      "pinned:existing",
                      "md5_pinned_existing",
                      true,
                      false),
    // Cache resource with a non-existent source file that is pinned.
    TestCacheResource("",
                      "pinned:non-existent",
                      "md5_pinned_non_existent",
                      true,
                      false),
    // Cache resource that is dirty.
    TestCacheResource("dirty/cache.avi",
                      "dirty:existing",
                      "md5_dirty_existing",
                      false,
                      true),
    // Cache resource that is pinned and dirty.
    TestCacheResource("pinned/dirty/cache.pdf",
                      "dirty_and_pinned:existing",
                      "md5_dirty_and_pinned_existing",
                      true,
                      true)
  };
  return std::vector<TestCacheResource>(
      resources,
      resources + ARRAYSIZE_UNSAFE(resources));
}

bool PrepareTestCacheResources(
    internal::FileCache* cache,
    const std::vector<TestCacheResource>& resources) {
  // cache->StoreOnUIThread requires real file to be stored. As a dummy data for
  // testing, an empty temporary file is created.
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return false;
  base::FilePath source_path;
  if (!file_util::CreateTemporaryFileInDir(temp_dir.path(), &source_path))
    return false;

  for (size_t i = 0; i < resources.size(); ++i) {
    // Copy file from data dir to cache.
    if (!resources[i].source_file.empty()) {
      FileError error = FILE_ERROR_OK;
      cache->StoreOnUIThread(
          resources[i].resource_id,
          resources[i].md5,
          source_path,
          internal::FileCache::FILE_OPERATION_COPY,
          google_apis::test_util::CreateCopyResultCallback(&error));
      google_apis::test_util::RunBlockingPoolTask();
      if (error != FILE_ERROR_OK)
        return false;
    }
    // Pin.
    if (resources[i].is_pinned) {
      FileError error = FILE_ERROR_OK;
      cache->PinOnUIThread(
          resources[i].resource_id,
          google_apis::test_util::CreateCopyResultCallback(&error));
      google_apis::test_util::RunBlockingPoolTask();
      if (error != FILE_ERROR_OK)
        return false;
    }
    // Mark dirty.
    if (resources[i].is_dirty) {
      FileError error = FILE_ERROR_OK;
      cache->MarkDirtyOnUIThread(
          resources[i].resource_id,
          resources[i].md5,
          google_apis::test_util::CreateCopyResultCallback(&error));
      google_apis::test_util::RunBlockingPoolTask();
      if (error != FILE_ERROR_OK)
        return false;
    }
  }
  return true;
}

FakeNetworkChangeNotifier::FakeNetworkChangeNotifier()
    : type_(CONNECTION_WIFI) {
}

void FakeNetworkChangeNotifier::SetConnectionType(ConnectionType type) {
  type_ = type;
  NotifyObserversOfConnectionTypeChange();
}

net::NetworkChangeNotifier::ConnectionType
FakeNetworkChangeNotifier::GetCurrentConnectionType() const {
  return type_;
}

}  // namespace test_util
}  // namespace drive
