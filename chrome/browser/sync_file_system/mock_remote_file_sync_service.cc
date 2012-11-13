// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/mock_remote_file_sync_service.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_url.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace sync_file_system {

MockRemoteFileSyncService::MockRemoteFileSyncService() {
  typedef MockRemoteFileSyncService self;
  ON_CALL(*this, RegisterOriginForTrackingChanges(_, _))
      .WillByDefault(Invoke(this, &self::RegisterOriginForTrackingChangesStub));
  ON_CALL(*this, UnregisterOriginForTrackingChanges(_, _))
      .WillByDefault(
          Invoke(this, &self::UnregisterOriginForTrackingChangesStub));
  ON_CALL(*this, ProcessRemoteChange(_, _))
      .WillByDefault(Invoke(this, &self::ProcessRemoteChangeStub));
  ON_CALL(*this, GetLocalChangeProcessor())
      .WillByDefault(Return(local_change_processor_.get()));
  ON_CALL(*this, GetConflictFiles(_, _))
      .WillByDefault(Invoke(this, &self::GetConflictFilesStub));
  ON_CALL(*this, GetRemoteFileMetadata(_, _))
      .WillByDefault(Invoke(this, &self::GetRemoteFileMetadataStub));
}

MockRemoteFileSyncService::~MockRemoteFileSyncService() {
}

void MockRemoteFileSyncService::RegisterOriginForTrackingChangesStub(
    const GURL& origin,
    const fileapi::SyncStatusCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, fileapi::SYNC_STATUS_OK));
}

void MockRemoteFileSyncService::UnregisterOriginForTrackingChangesStub(
    const GURL& origin,
    const fileapi::SyncStatusCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, fileapi::SYNC_STATUS_OK));
}

void MockRemoteFileSyncService::ProcessRemoteChangeStub(
    RemoteChangeProcessor* processor,
    const fileapi::SyncFileCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, fileapi::SYNC_STATUS_NO_CHANGE_TO_SYNC,
                 fileapi::FileSystemURL()));
}

void MockRemoteFileSyncService::GetConflictFilesStub(
    const GURL& origin,
    const fileapi::SyncFileSetCallback& callback) {
  fileapi::FileSystemURLSet urls;
  OriginToURLSetMap::iterator iter = conflict_file_urls_.find(origin);
  if (iter != conflict_file_urls_.end())
    urls.insert(iter->second.begin(), iter->second.end());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, fileapi::SYNC_STATUS_OK, urls));
}

void MockRemoteFileSyncService::GetRemoteFileMetadataStub(
    const fileapi::FileSystemURL& url,
    const fileapi::SyncFileMetadataCallback& callback) {
  FileMetadataMap::iterator iter = conflict_file_metadata_.find(url);
  if (iter == conflict_file_metadata_.end()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, fileapi::SYNC_FILE_ERROR_NOT_FOUND,
                   fileapi::SyncFileMetadata()));
    return;
  }
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, fileapi::SYNC_STATUS_OK, iter->second));
}

}  // namespace sync_file_system
