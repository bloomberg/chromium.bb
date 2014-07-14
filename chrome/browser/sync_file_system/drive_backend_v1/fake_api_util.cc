// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/fake_api_util.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "webkit/common/blob/scoped_file.h"

namespace sync_file_system {
namespace drive_backend {

bool FakeAPIUtil::RemoteResourceComparator::operator()(
    const RemoteResource& left,
    const RemoteResource& right) {
  if (left.parent_resource_id != right.parent_resource_id)
    return left.parent_resource_id < right.parent_resource_id;
  if (left.parent_title != right.parent_title)
    return left.parent_title < right.parent_title;
  if (left.title != right.title)
    return left.title < right.title;
  if (left.resource_id != right.resource_id)
    return left.resource_id < right.resource_id;
  if (left.md5_checksum != right.md5_checksum)
    return left.md5_checksum < right.md5_checksum;
  if (left.deleted != right.deleted)
    return left.deleted < right.deleted;
  return left.changestamp < right.changestamp;
}

struct FakeAPIUtil::ChangeStampComparator {
  bool operator()(const google_apis::ResourceEntry* left,
                  const google_apis::ResourceEntry* right) {
    return left->changestamp() < right->changestamp();
  }
};

FakeAPIUtil::RemoteResource::RemoteResource()
    : type(SYNC_FILE_TYPE_UNKNOWN), deleted(false), changestamp(0) {}

FakeAPIUtil::RemoteResource::RemoteResource(
    const std::string& parent_resource_id,
    const std::string& parent_title,
    const std::string& title,
    const std::string& resource_id,
    const std::string& md5_checksum,
    SyncFileType type,
    bool deleted,
    int64 changestamp)
    : parent_resource_id(parent_resource_id),
      parent_title(parent_title),
      title(title),
      resource_id(resource_id),
      md5_checksum(md5_checksum),
      type(type),
      deleted(deleted),
      changestamp(changestamp) {}

FakeAPIUtil::RemoteResource::~RemoteResource() {}

FakeAPIUtil::FakeAPIUtil()
    : largest_changestamp_(0),
      url_generator_(
          GURL(google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction)) {}

FakeAPIUtil::~FakeAPIUtil() {}

void FakeAPIUtil::AddObserver(APIUtilObserver* observer) {}

void FakeAPIUtil::RemoveObserver(APIUtilObserver* observer) {}

void FakeAPIUtil::GetDriveDirectoryForSyncRoot(
    const ResourceIdCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 google_apis::HTTP_SUCCESS,
                 "folder: sync_root_resource_id"));
}

void FakeAPIUtil::GetDriveDirectoryForOrigin(
    const std::string& sync_root_resource_id,
    const GURL& origin,
    const ResourceIdCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 google_apis::HTTP_SUCCESS,
                 "folder resource_id for " + origin.host()));
}

void FakeAPIUtil::GetLargestChangeStamp(const ChangeStampCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS, largest_changestamp_));
}

void FakeAPIUtil::GetResourceEntry(const std::string& resource_id,
                                   const ResourceEntryCallback& callback) {
  NOTREACHED();
}

void FakeAPIUtil::ListFiles(const std::string& directory_resource_id,
                            const ResourceListCallback& callback) {
  ListChanges(0, callback);
}

void FakeAPIUtil::ListChanges(int64 start_changestamp,
                              const ResourceListCallback& callback) {
  scoped_ptr<google_apis::ResourceList> change_feed(
      new google_apis::ResourceList());

  ScopedVector<google_apis::ResourceEntry> entries;
  typedef RemoteResourceByResourceId::const_iterator iterator;
  for (iterator itr = remote_resources_.begin();
       itr != remote_resources_.end(); ++itr) {
    if (itr->second.changestamp < start_changestamp)
      continue;
    scoped_ptr<google_apis::ResourceEntry> entry(
        CreateResourceEntry(itr->second));
    entries.push_back(entry.release());
  }

  std::sort(entries.begin(), entries.end(), ChangeStampComparator());

  change_feed->set_entries(entries.Pass());
  change_feed->set_largest_changestamp(largest_changestamp_);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(
          callback, google_apis::HTTP_SUCCESS, base::Passed(&change_feed)));
}

void FakeAPIUtil::ContinueListing(const GURL& next_link,
                                  const ResourceListCallback& callback) {
  NOTREACHED();
}

void FakeAPIUtil::DownloadFile(const std::string& resource_id,
                               const std::string& local_file_md5,
                               const DownloadFileCallback& callback) {
  RemoteResourceByResourceId::iterator found =
      remote_resources_.find(resource_id);
  std::string file_md5;
  int64 file_size = 0;
  base::Time updated_time;
  google_apis::GDataErrorCode error = google_apis::HTTP_NOT_FOUND;

  if (found != remote_resources_.end() && !found->second.deleted) {
    scoped_ptr<google_apis::ResourceEntry> entry(
        CreateResourceEntry(found->second));
    file_md5 = entry->file_md5();
    file_size = entry->file_size();
    updated_time = entry->updated_time();
    error = google_apis::HTTP_SUCCESS;
  }

  webkit_blob::ScopedFile dummy;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, file_md5, file_size, updated_time,
                 base::Passed(&dummy)));
}

void FakeAPIUtil::UploadNewFile(const std::string& directory_resource_id,
                                const base::FilePath& local_file_path,
                                const std::string& title,
                                const UploadFileCallback& callback) {
  NOTREACHED();
}

void FakeAPIUtil::UploadExistingFile(const std::string& resource_id,
                                     const std::string& remote_file_md5,
                                     const base::FilePath& local_file_path,
                                     const UploadFileCallback& callback) {
  NOTREACHED();
}

void FakeAPIUtil::CreateDirectory(const std::string& parent_resource_id,
                                  const std::string& title,
                                  const ResourceIdCallback& callback) {
  NOTREACHED();
}

bool FakeAPIUtil::IsAuthenticated() const { return true; }

void FakeAPIUtil::DeleteFile(const std::string& resource_id,
                             const std::string& remote_file_md5,
                             const GDataErrorCallback& callback) {
  if (!ContainsKey(remote_resources_, resource_id)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, google_apis::HTTP_NOT_FOUND));
    return;
  }

  const RemoteResource& deleted_directory = remote_resources_[resource_id];
  PushRemoteChange(deleted_directory.parent_resource_id,
                   deleted_directory.parent_title,
                   deleted_directory.title,
                   deleted_directory.resource_id,
                   deleted_directory.md5_checksum,
                   SYNC_FILE_TYPE_UNKNOWN,
                   true /* deleted */);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS));
}

void FakeAPIUtil::EnsureSyncRootIsNotInMyDrive(
    const std::string& sync_root_resource_id) {
  // Nothing to do.
}

void FakeAPIUtil::PushRemoteChange(const std::string& parent_resource_id,
                                   const std::string& parent_title,
                                   const std::string& title,
                                   const std::string& resource_id,
                                   const std::string& md5,
                                   SyncFileType type,
                                   bool deleted) {
  remote_resources_[resource_id] = RemoteResource(
      parent_resource_id, parent_title, title, resource_id,
      md5, type, deleted, ++largest_changestamp_);
}

scoped_ptr<google_apis::ResourceEntry> FakeAPIUtil::CreateResourceEntry(
    const RemoteResource& resource) const {
  scoped_ptr<google_apis::ResourceEntry> entry(
      new google_apis::ResourceEntry());
  ScopedVector<google_apis::Link> parent_links;
  scoped_ptr<google_apis::Link> link(new google_apis::Link());

  link->set_type(google_apis::Link::LINK_PARENT);
  link->set_href(ResourceIdToResourceLink(resource.parent_resource_id));
  link->set_title(resource.parent_title);
  parent_links.push_back(link.release());

  entry->set_links(parent_links.Pass());
  entry->set_title(resource.title);
  entry->set_resource_id(resource.resource_id);
  entry->set_file_md5(resource.md5_checksum);
  entry->set_deleted(resource.deleted);
  entry->set_changestamp(resource.changestamp);

  switch (resource.type) {
    case SYNC_FILE_TYPE_FILE:
      entry->set_kind(google_apis::ResourceEntry::ENTRY_KIND_FILE);
      break;
    case SYNC_FILE_TYPE_DIRECTORY:
      entry->set_kind(google_apis::ResourceEntry::ENTRY_KIND_FOLDER);
      break;
    case SYNC_FILE_TYPE_UNKNOWN:
      entry->set_kind(google_apis::ResourceEntry::ENTRY_KIND_UNKNOWN);
      break;
  }

  return entry.Pass();
}

GURL FakeAPIUtil::ResourceIdToResourceLink(
    const std::string& resource_id) const {
  return url_generator_.GenerateEditUrl(resource_id);
}

}  // namespace drive_backend
}  // namespace sync_file_system
