// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/mock_remote_file_sync_service.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_url.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace sync_file_system {

MockRemoteFileSyncService::MockRemoteFileSyncService()
    : conflict_resolution_policy_(CONFLICT_RESOLUTION_POLICY_MANUAL),
      state_(REMOTE_SERVICE_OK) {
  typedef MockRemoteFileSyncService self;
  ON_CALL(*this, AddServiceObserver(_))
      .WillByDefault(Invoke(this, &self::AddServiceObserverStub));
  ON_CALL(*this, AddFileStatusObserver(_))
      .WillByDefault(Invoke(this, &self::AddFileStatusObserverStub));
  ON_CALL(*this, RegisterOrigin(_, _))
      .WillByDefault(Invoke(this, &self::RegisterOriginStub));
  ON_CALL(*this, UninstallOrigin(_, _, _))
      .WillByDefault(
          Invoke(this, &self::DeleteOriginDirectoryStub));
  ON_CALL(*this, ProcessRemoteChange(_))
      .WillByDefault(Invoke(this, &self::ProcessRemoteChangeStub));
  ON_CALL(*this, GetLocalChangeProcessor())
      .WillByDefault(Return(&mock_local_change_processor_));
  ON_CALL(*this, GetCurrentState())
      .WillByDefault(Invoke(this, &self::GetCurrentStateStub));
}

MockRemoteFileSyncService::~MockRemoteFileSyncService() {
}

void MockRemoteFileSyncService::DumpFiles(const GURL& origin,
                                          const ListCallback& callback) {
  callback.Run(scoped_ptr<base::ListValue>());
}

void MockRemoteFileSyncService::DumpDatabase(const ListCallback& callback) {
  callback.Run(scoped_ptr<base::ListValue>());
}

void MockRemoteFileSyncService::SetServiceState(RemoteServiceState state) {
  state_ = state;
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

void MockRemoteFileSyncService::RegisterOriginStub(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, SYNC_STATUS_OK));
}

void MockRemoteFileSyncService::DeleteOriginDirectoryStub(
    const GURL& origin,
    UninstallFlag flag,
    const SyncStatusCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, SYNC_STATUS_OK));
}

void MockRemoteFileSyncService::ProcessRemoteChangeStub(
    const SyncFileCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, SYNC_STATUS_NO_CHANGE_TO_SYNC,
                 fileapi::FileSystemURL()));
}

RemoteServiceState MockRemoteFileSyncService::GetCurrentStateStub() const {
  return state_;
}

}  // namespace sync_file_system
