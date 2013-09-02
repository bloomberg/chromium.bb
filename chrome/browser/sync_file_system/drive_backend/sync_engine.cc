// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"

#include "base/values.h"
#include "chrome/browser/drive/drive_api_service.h"

namespace sync_file_system {
namespace drive_backend {

SyncEngine::SyncEngine(
    const base::FilePath& base_dir,
    scoped_ptr<drive::DriveAPIService> drive_api,
    drive::DriveNotificationManager* notification_manager,
    ExtensionService* extension_service)
    : base_dir_(base_dir),
      drive_api_(drive_api.Pass()),
      notification_manager_(notification_manager),
      extension_service_(extension_service),
      weak_ptr_factory_(this),
      task_manager_(weak_ptr_factory_.GetWeakPtr()) {
}

SyncEngine::~SyncEngine() {
  NOTIMPLEMENTED();
}

void SyncEngine::Initialize(const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void SyncEngine::AddServiceObserver(SyncServiceObserver* observer) {
  service_observers_.AddObserver(observer);
}

void SyncEngine::AddFileStatusObserver(FileStatusObserver* observer) {
  file_status_observers_.AddObserver(observer);
}

void SyncEngine::RegisterOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void SyncEngine::UnregisterOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void SyncEngine::EnableOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void SyncEngine::DisableOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void SyncEngine::UninstallOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void SyncEngine::ProcessRemoteChange(
    const SyncFileCallback& callback) {
  NOTIMPLEMENTED();
}

void SyncEngine::SetRemoteChangeProcessor(
    RemoteChangeProcessor* processor) {
  remote_change_processor_ = processor;
}

LocalChangeProcessor* SyncEngine::GetLocalChangeProcessor() {
  return this;
}

bool SyncEngine::IsConflicting(const fileapi::FileSystemURL& url) {
  NOTIMPLEMENTED();
  return false;
}

RemoteServiceState SyncEngine::GetCurrentState() const {
  NOTIMPLEMENTED();
  return REMOTE_SERVICE_OK;
}

void SyncEngine::GetOriginStatusMap(OriginStatusMap* status_map) {
  DCHECK(status_map);
  status_map->clear();
  NOTIMPLEMENTED();
}

scoped_ptr<base::ListValue> SyncEngine::DumpFiles(const GURL& origin) {
  NOTIMPLEMENTED();
  return make_scoped_ptr(new base::ListValue());
}

void SyncEngine::SetSyncEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

SyncStatusCode SyncEngine::SetConflictResolutionPolicy(
    ConflictResolutionPolicy policy) {
  NOTIMPLEMENTED();
  return SYNC_STATUS_OK;
}

ConflictResolutionPolicy
SyncEngine::GetConflictResolutionPolicy() const {
  NOTIMPLEMENTED();
  return CONFLICT_RESOLUTION_POLICY_LAST_WRITE_WIN;
}

void SyncEngine::GetRemoteVersions(
    const fileapi::FileSystemURL& url,
    const RemoteVersionsCallback& callback) {
  NOTIMPLEMENTED();
}

void SyncEngine::DownloadRemoteVersion(
    const fileapi::FileSystemURL& url,
    const std::string& version_id,
    const DownloadVersionCallback& callback) {
  NOTIMPLEMENTED();
}

void SyncEngine::ApplyLocalChange(
    const FileChange& local_file_change,
    const base::FilePath& local_file_path,
    const SyncFileMetadata& local_file_metadata,
    const fileapi::FileSystemURL& url,
    const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void SyncEngine::MaybeScheduleNextTask() {
  NOTIMPLEMENTED();
}

void SyncEngine::NotifyLastOperationStatus(SyncStatusCode sync_status) {
  NOTIMPLEMENTED();
}

void SyncEngine::OnNotificationReceived() {
  NOTIMPLEMENTED();
}

void SyncEngine::OnPushNotificationEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

}  // namespace drive_backend
}  // namespace sync_file_system
