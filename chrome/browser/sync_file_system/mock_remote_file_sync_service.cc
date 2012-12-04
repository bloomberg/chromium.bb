// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/mock_remote_file_sync_service.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/sync_operation_type.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace sync_file_system {

MockRemoteFileSyncService::MockRemoteFileSyncService() {
  typedef MockRemoteFileSyncService self;
  ON_CALL(*this, AddObserver(_))
      .WillByDefault(Invoke(this, &self::AddObserverStub));
  ON_CALL(*this, RemoveObserver(_))
      .WillByDefault(Invoke(this, &self::RemoveObserverStub));
  ON_CALL(*this, RegisterOriginForTrackingChanges(_, _))
      .WillByDefault(Invoke(this, &self::RegisterOriginForTrackingChangesStub));
  ON_CALL(*this, UnregisterOriginForTrackingChanges(_, _))
      .WillByDefault(
          Invoke(this, &self::UnregisterOriginForTrackingChangesStub));
  ON_CALL(*this, ProcessRemoteChange(_, _))
      .WillByDefault(Invoke(this, &self::ProcessRemoteChangeStub));
  ON_CALL(*this, GetLocalChangeProcessor())
      .WillByDefault(Return(&mock_local_change_processor_));
  ON_CALL(*this, IsConflicting(_))
      .WillByDefault(Return(false));
  ON_CALL(*this, GetConflictFiles(_, _))
      .WillByDefault(Invoke(this, &self::GetConflictFilesStub));
  ON_CALL(*this, GetRemoteFileMetadata(_, _))
      .WillByDefault(Invoke(this, &self::GetRemoteFileMetadataStub));
  ON_CALL(*this, GetCurrentState())
      .WillByDefault(Return(REMOTE_SERVICE_OK));
}

MockRemoteFileSyncService::~MockRemoteFileSyncService() {
}

void MockRemoteFileSyncService::NotifyRemoteChangeAvailable(
    int64 pending_changes) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnRemoteChangeAvailable(pending_changes));
}

void MockRemoteFileSyncService::NotifyRemoteServiceStateUpdated(
    RemoteServiceState state,
    const std::string& description) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnRemoteServiceStateUpdated(state, description));
}

void MockRemoteFileSyncService::AddObserverStub(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockRemoteFileSyncService::RemoveObserverStub(Observer* observer) {
  observers_.RemoveObserver(observer);
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
    const fileapi::SyncOperationCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, fileapi::SYNC_STATUS_NO_CHANGE_TO_SYNC,
                 fileapi::FileSystemURL(),
                 fileapi::SYNC_OPERATION_NONE));
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
