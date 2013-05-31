// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/test_util.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/threading/worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/change_list_loader.h"
#include "chrome/browser/chromeos/drive/change_list_processor.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/google_apis/drive_api_parser.h"

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
    TestCacheResource("gdata/root_feed.json",
                      "tmp:resource_id",
                      "md5_tmp_alphanumeric",
                      false,
                      false),
    // Cache resource in tmp dir, i.e. not pinned or dirty, with resource_id
    // containing non-alphanumeric characters.
    TestCacheResource("gdata/empty_feed.json",
                      "tmp:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?",
                      "md5_tmp_non_alphanumeric",
                      false,
                      false),
    // Cache resource that is pinned and persistent.
    TestCacheResource("gdata/directory_entry.json",
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
    TestCacheResource("gdata/account_metadata.json",
                      "dirty:existing",
                      "md5_dirty_existing",
                      false,
                      true),
    // Cache resource that is pinned and dirty.
    TestCacheResource("gdata/basic_feed.json",
                      "dirty_and_pinned:existing",
                      "md5_dirty_and_pinned_existing",
                      true,
                      true)
  };
  return std::vector<TestCacheResource>(
      resources,
      resources + ARRAYSIZE_UNSAFE(resources));
}

FileCacheEntry ToCacheEntry(int cache_state) {
  FileCacheEntry cache_entry;
  cache_entry.set_is_present(cache_state & TEST_CACHE_STATE_PRESENT);
  cache_entry.set_is_pinned(cache_state & TEST_CACHE_STATE_PINNED);
  cache_entry.set_is_dirty(cache_state & TEST_CACHE_STATE_DIRTY);
  cache_entry.set_is_mounted(cache_state & TEST_CACHE_STATE_MOUNTED);
  cache_entry.set_is_persistent(cache_state & TEST_CACHE_STATE_PERSISTENT);
  return cache_entry;
}

bool CacheStatesEqual(const FileCacheEntry& a, const FileCacheEntry& b) {
  return (a.is_present() == b.is_present() &&
          a.is_pinned() == b.is_pinned() &&
          a.is_dirty() == b.is_dirty() &&
          a.is_mounted() == b.is_mounted() &&
          a.is_persistent() == b.is_persistent());
}

bool LoadChangeFeed(const std::string& relative_path,
                    internal::ChangeListLoader* change_list_loader,
                    bool is_delta_feed,
                    const std::string& root_resource_id,
                    int64 root_feed_changestamp) {
  scoped_ptr<Value> document =
      google_apis::test_util::LoadJSONFile(relative_path);
  if (!document.get())
    return false;
  if (document->GetType() != Value::TYPE_DICTIONARY)
    return false;

  scoped_ptr<google_apis::ResourceList> document_feed(
      google_apis::ResourceList::ExtractAndParse(*document));
  if (!document_feed.get())
    return false;

  ScopedVector<internal::ChangeList> change_lists;
  change_lists.push_back(new internal::ChangeList(*document_feed));

  scoped_ptr<google_apis::AboutResource> about_resource(
      new google_apis::AboutResource);
  about_resource->set_largest_change_id(root_feed_changestamp);
  about_resource->set_root_folder_id(root_resource_id);

  change_list_loader->UpdateFromChangeList(
      about_resource.Pass(),
      change_lists.Pass(),
      is_delta_feed,
      base::Bind(&base::DoNothing));
  google_apis::test_util::RunBlockingPoolTask();

  return true;
}

bool PrepareTestCacheResources(
    internal::FileCache* cache,
    const std::vector<TestCacheResource>& resources) {
  for (size_t i = 0; i < resources.size(); ++i) {
    // Copy file from data dir to cache.
    if (!resources[i].source_file.empty()) {
      base::FilePath source_path =
          google_apis::test_util::GetTestFilePath(
              std::string("chromeos/") + resources[i].source_file);

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
          resources[i].md5,
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

      cache->CommitDirtyOnUIThread(
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

}  // namespace test_util
}  // namespace drive
