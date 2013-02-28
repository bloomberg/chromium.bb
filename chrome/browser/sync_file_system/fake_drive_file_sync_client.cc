// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/fake_drive_file_sync_client.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"

namespace sync_file_system {

bool FakeDriveFileSyncClient::RemoteResourceComparator::operator()(
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

struct FakeDriveFileSyncClient::ChangeStampComparator {
  bool operator()(const google_apis::ResourceEntry* left,
                  const google_apis::ResourceEntry* right) {
    return left->changestamp() < right->changestamp();
  }
};

FakeDriveFileSyncClient::RemoteResource::RemoteResource()
    : deleted(false),
      changestamp(0) {
}

FakeDriveFileSyncClient::RemoteResource::RemoteResource(
    const std::string& parent_resource_id,
    const std::string& parent_title,
    const std::string& title,
    const std::string& resource_id,
    const std::string& md5_checksum,
    bool deleted,
    int64 changestamp)
    : parent_resource_id(parent_resource_id),
      parent_title(parent_title),
      title(title),
      resource_id(resource_id),
      md5_checksum(md5_checksum),
      deleted(deleted),
      changestamp(changestamp) {
}

FakeDriveFileSyncClient::RemoteResource::~RemoteResource() {
}

FakeDriveFileSyncClient::FakeDriveFileSyncClient()
    : largest_changestamp_(0),
      url_generator_(GURL(
          google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction)) {
}

FakeDriveFileSyncClient::~FakeDriveFileSyncClient() {
}

void FakeDriveFileSyncClient::AddObserver(
    DriveFileSyncClientObserver* observer) {
}

void FakeDriveFileSyncClient::RemoveObserver(
    DriveFileSyncClientObserver* observer) {
}

void FakeDriveFileSyncClient::GetDriveDirectoryForSyncRoot(
    const ResourceIdCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS,
                 "folder: sync_root_resource_id"));
}

void FakeDriveFileSyncClient::GetDriveDirectoryForOrigin(
    const std::string& sync_root_resource_id,
    const GURL& origin,
    const ResourceIdCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS,
                 "folder resource_id for " + origin.host()));
}

void FakeDriveFileSyncClient::GetLargestChangeStamp(
    const ChangeStampCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS, largest_changestamp_));
}

void FakeDriveFileSyncClient::GetResourceEntry(
    const std::string& resource_id,
    const ResourceEntryCallback& callback) {
  NOTREACHED();
}

void FakeDriveFileSyncClient::ListFiles(
    const std::string& directory_resource_id,
    const ResourceListCallback& callback) {
  NOTREACHED();
}

void FakeDriveFileSyncClient::ListChanges(
    int64 start_changestamp,
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

  std::sort(entries.begin(), entries.end(),
            ChangeStampComparator());

  change_feed->set_entries(&entries);
  change_feed->set_largest_changestamp(largest_changestamp_);

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS,
                 base::Passed(&change_feed)));
}

void FakeDriveFileSyncClient::ContinueListing(
    const GURL& feed_url,
    const ResourceListCallback& callback) {
  NOTREACHED();
}

void FakeDriveFileSyncClient::DownloadFile(
    const std::string& resource_id,
    const std::string& local_file_md5,
    const base::FilePath& local_file_path,
    const DownloadFileCallback& callback) {
  RemoteResourceByResourceId::iterator found =
      remote_resources_.find(resource_id);
  std::string file_md5;
  google_apis::GDataErrorCode error = google_apis::HTTP_NOT_FOUND;

  if (found != remote_resources_.end() && !found->second.deleted) {
    scoped_ptr<google_apis::ResourceEntry> entry(
        CreateResourceEntry(found->second));
    file_md5 = entry->file_md5();
    error = google_apis::HTTP_SUCCESS;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, file_md5));
}

void FakeDriveFileSyncClient::UploadNewFile(
    const std::string& directory_resource_id,
    const base::FilePath& local_file_path,
    const std::string& title,
    const UploadFileCallback& callback) {
  NOTREACHED();
}

void FakeDriveFileSyncClient::UploadExistingFile(
    const std::string& resource_id,
    const std::string& remote_file_md5,
    const base::FilePath& local_file_path,
    const UploadFileCallback& callback) {
  NOTREACHED();
}

bool FakeDriveFileSyncClient::IsAuthenticated() const {
  return true;
}

void FakeDriveFileSyncClient::DeleteFile(
    const std::string& resource_id,
    const std::string& remote_file_md5,
    const GDataErrorCallback& callback) {
  NOTREACHED();
}

GURL FakeDriveFileSyncClient::ResourceIdToResourceLink(
    const std::string& resource_id) const {
  return url_generator_.GenerateContentUrl(resource_id);
}

void FakeDriveFileSyncClient::PushRemoteChange(
    const std::string& parent_resource_id,
    const std::string& parent_title,
    const std::string& title,
    const std::string& resource_id,
    const std::string& md5,
    bool deleted) {
  remote_resources_[resource_id] = RemoteResource(
      parent_resource_id, parent_title, title, resource_id,
      md5, deleted, ++largest_changestamp_);
}

scoped_ptr<google_apis::ResourceEntry>
FakeDriveFileSyncClient::CreateResourceEntry(
    const RemoteResource& resource) const {
  scoped_ptr<google_apis::ResourceEntry> entry(
      new google_apis::ResourceEntry());
  ScopedVector<google_apis::Link> parent_links;
  scoped_ptr<google_apis::Link> link(new google_apis::Link());

  link->set_type(google_apis::Link::LINK_PARENT);
  link->set_href(url_generator_.GenerateContentUrl(
      resource.parent_resource_id));
  link->set_title(resource.parent_title);
  parent_links.push_back(link.release());

  entry->set_links(&parent_links);
  entry->set_title(resource.title);
  entry->set_resource_id(resource.resource_id);
  entry->set_file_md5(resource.md5_checksum);
  entry->set_deleted(resource.deleted);
  entry->set_changestamp(resource.changestamp);

  return entry.Pass();
}

}  // namespace sync_file_system
