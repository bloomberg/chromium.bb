// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/mock_remote_file_sync_service.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_url.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace sync_file_system {

const char MockRemoteFileSyncService::kServiceName[] = "mock_sync_service";

MockRemoteFileSyncService::MockRemoteFileSyncService() {
  typedef MockRemoteFileSyncService self;
  ON_CALL(*this, AddServiceObserver(_))
      .WillByDefault(Invoke(this, &self::AddServiceObserverStub));
  ON_CALL(*this, AddFileStatusObserver(_))
      .WillByDefault(Invoke(this, &self::AddFileStatusObserverStub));
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
  ON_CALL(*this, GetRemoteFileMetadata(_, _))
      .WillByDefault(Invoke(this, &self::GetRemoteFileMetadataStub));
  ON_CALL(*this, GetCurrentState())
      .WillByDefault(Return(REMOTE_SERVICE_OK));
  ON_CALL(*this, GetServiceName())
      .WillByDefault(Return(kServiceName));
}

MockRemoteFileSyncService::~MockRemoteFileSyncService() {
}

void MockRemoteFileSyncService::NotifyRemoteChangeQueueUpdated(
    int64 pending_changes) {
  FOR_EACH_OBSERVER(Observer, service_observers_,
                    OnRemoteChangeQueueUpdated(pending_changes));
}

void MockRemoteFileSyncService::NotifyRemoteServiceStateUpdated(
    RemoteServiceState state,
    const std::string& description) {
  FOR_EACH_OBSERVER(Observer, service_observers_,
                    OnRemoteServiceStateUpdated(state, description));
}

void MockRemoteFileSyncService::NotifyFileStatusChanged(
    const fileapi::FileSystemURL& url,
    SyncFileStatus sync_status,
    SyncAction action_taken,
    SyncDirection direction) {
  FOR_EACH_OBSERVER(FileStatusObserver, file_status_observers_,
                    OnFileStatusChanged(url, sync_status,
                                        action_taken, direction));
}

void MockRemoteFileSyncService::AddServiceObserverStub(Observer* observer) {
  service_observers_.AddObserver(observer);
}

void MockRemoteFileSyncService::AddFileStatusObserverStub(
    FileStatusObserver* observer) {
  file_status_observers_.AddObserver(observer);
}

void MockRemoteFileSyncService::RegisterOriginForTrackingChangesStub(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, SYNC_STATUS_OK));
}

void MockRemoteFileSyncService::UnregisterOriginForTrackingChangesStub(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, SYNC_STATUS_OK));
}

void MockRemoteFileSyncService::ProcessRemoteChangeStub(
    RemoteChangeProcessor* processor,
    const SyncFileCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, SYNC_STATUS_NO_CHANGE_TO_SYNC,
                 fileapi::FileSystemURL()));
}

void MockRemoteFileSyncService::GetRemoteFileMetadataStub(
    const fileapi::FileSystemURL& url,
    const SyncFileMetadataCallback& callback) {
  FileMetadataMap::iterator iter = conflict_file_metadata_.find(url);
  if (iter == conflict_file_metadata_.end()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_FILE_ERROR_NOT_FOUND,
                   SyncFileMetadata()));
    return;
  }
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, SYNC_STATUS_OK, iter->second));
}

}  // namespace sync_file_system
