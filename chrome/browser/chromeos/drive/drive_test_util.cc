// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_test_util.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/drive/change_list_loader.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/google_apis/drive_api_parser.h"

namespace drive {

namespace test_util {

DriveCacheEntry ToCacheEntry(int cache_state) {
  DriveCacheEntry cache_entry;
  cache_entry.set_is_present(cache_state & TEST_CACHE_STATE_PRESENT);
  cache_entry.set_is_pinned(cache_state & TEST_CACHE_STATE_PINNED);
  cache_entry.set_is_dirty(cache_state & TEST_CACHE_STATE_DIRTY);
  cache_entry.set_is_mounted(cache_state & TEST_CACHE_STATE_MOUNTED);
  cache_entry.set_is_persistent(cache_state & TEST_CACHE_STATE_PERSISTENT);
  return cache_entry;
}

bool CacheStatesEqual(const DriveCacheEntry& a, const DriveCacheEntry& b) {
  return (a.is_present() == b.is_present() &&
          a.is_pinned() == b.is_pinned() &&
          a.is_dirty() == b.is_dirty() &&
          a.is_mounted() == b.is_mounted() &&
          a.is_persistent() == b.is_persistent());
}

void CopyResultsFromGetAvailableSpaceCallback(DriveFileError* out_error,
                                              int64* out_bytes_total,
                                              int64* out_bytes_used,
                                              DriveFileError error,
                                              int64 bytes_total,
                                              int64 bytes_used) {
  DCHECK(out_error);
  DCHECK(out_bytes_total);
  DCHECK(out_bytes_used);
  *out_error = error;
  *out_bytes_total = bytes_total;
  *out_bytes_used = bytes_used;
}

void CopyResultsFromSearchMetadataCallback(
    DriveFileError* out_error,
    scoped_ptr<MetadataSearchResultVector>* out_result,
    DriveFileError error,
    scoped_ptr<MetadataSearchResultVector> result) {
  DCHECK(out_error);
  DCHECK(out_result);

  *out_error = error;
  *out_result = result.Pass();
}

void CopyResultsFromOpenFileCallbackAndQuit(DriveFileError* out_error,
                                            base::FilePath* out_file_path,
                                            DriveFileError error,
                                            const base::FilePath& file_path) {
  *out_error = error;
  *out_file_path = file_path;
  MessageLoop::current()->Quit();
}

void CopyResultsFromCloseFileCallbackAndQuit(DriveFileError* out_error,
                                             DriveFileError error) {
  *out_error = error;
  MessageLoop::current()->Quit();
}

bool LoadChangeFeed(const std::string& relative_path,
                    ChangeListLoader* change_list_loader,
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

  ScopedVector<google_apis::ResourceList> feed_list;
  feed_list.push_back(document_feed.release());

  scoped_ptr<google_apis::AboutResource> about_resource(
      new google_apis::AboutResource);
  about_resource->set_largest_change_id(root_feed_changestamp);
  about_resource->set_root_folder_id(root_resource_id);

  change_list_loader->UpdateFromFeed(
      about_resource.Pass(),
      feed_list,
      is_delta_feed,
      base::Bind(&base::DoNothing));
  // ChangeListLoader::UpdateFromFeed is asynchronous, so wait for it to finish.
  google_apis::test_util::RunBlockingPoolTask();

  return true;
}

}  // namespace test_util
}  // namespace drive
