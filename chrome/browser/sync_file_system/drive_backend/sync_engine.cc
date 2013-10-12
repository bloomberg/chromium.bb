// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/drive/drive_api_service.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/local_to_remote_syncer.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/register_app_task.h"
#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"
#include "chrome/browser/sync_file_system/drive_backend/uninstall_app_task.h"
#include "chrome/browser/sync_file_system/sync_task.h"

namespace sync_file_system {
namespace drive_backend {

SyncEngine::SyncEngine(
    const base::FilePath& base_dir,
    base::SequencedTaskRunner* task_runner,
    scoped_ptr<drive::DriveAPIService> drive_service,
    drive::DriveNotificationManager* notification_manager,
    ExtensionService* extension_service)
    : base_dir_(base_dir),
      task_runner_(task_runner),
      drive_service_(drive_service.Pass()),
      notification_manager_(notification_manager),
      extension_service_(extension_service),
      weak_ptr_factory_(this),
      task_manager_(weak_ptr_factory_.GetWeakPtr()) {
}

SyncEngine::~SyncEngine() {
  NOTIMPLEMENTED();
}

void SyncEngine::Initialize() {
  task_manager_.Initialize(SYNC_STATUS_OK);

  SyncEngineInitializer* initializer =
      new SyncEngineInitializer(task_runner_.get(),
                                drive_service_.get(),
                                base_dir_.Append(kDatabaseName));
  task_manager_.ScheduleSyncTask(
      scoped_ptr<SyncTask>(initializer),
      base::Bind(&SyncEngine::DidInitialize, weak_ptr_factory_.GetWeakPtr(),
                 initializer));
}

void SyncEngine::AddServiceObserver(SyncServiceObserver* observer) {
  service_observers_.AddObserver(observer);
}

void SyncEngine::AddFileStatusObserver(FileStatusObserver* observer) {
  file_status_observers_.AddObserver(observer);
}

void SyncEngine::RegisterOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  task_manager_.ScheduleSyncTask(
      scoped_ptr<SyncTask>(new RegisterAppTask(this, origin.host())),
      callback);
}

void SyncEngine::EnableOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  task_manager_.ScheduleTask(
      base::Bind(&SyncEngine::DoEnableApp,
                 weak_ptr_factory_.GetWeakPtr(),
                 origin.host()),
      callback);
}

void SyncEngine::DisableOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  task_manager_.ScheduleTask(
      base::Bind(&SyncEngine::DoDisableApp,
                 weak_ptr_factory_.GetWeakPtr(),
                 origin.host()),
      callback);
}

void SyncEngine::UninstallOrigin(
    const GURL& origin,
    UninstallFlag flag,
    const SyncStatusCallback& callback) {
  task_manager_.ScheduleSyncTask(
      scoped_ptr<SyncTask>(new UninstallAppTask(this, origin.host(), flag)),
      callback);
}

void SyncEngine::ProcessRemoteChange(
    const SyncFileCallback& callback) {
  RemoteToLocalSyncer* syncer = new RemoteToLocalSyncer;
  task_manager_.ScheduleSyncTask(
      scoped_ptr<SyncTask>(syncer),
      base::Bind(&SyncEngine::DidProcessRemoteChange,
                 weak_ptr_factory_.GetWeakPtr(),
                 syncer, callback));
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
  LocalToRemoteSyncer* syncer = new LocalToRemoteSyncer;
  task_manager_.ScheduleSyncTask(
      scoped_ptr<SyncTask>(syncer),
      base::Bind(&SyncEngine::DidApplyLocalChange,
                 weak_ptr_factory_.GetWeakPtr(),
                 syncer, callback));
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

drive::DriveServiceInterface* SyncEngine::GetDriveService() {
  return drive_service_.get();
}

MetadataDatabase* SyncEngine::GetMetadataDatabase() {
  return metadata_database_.get();
}

void SyncEngine::DoDisableApp(const std::string& app_id,
                              const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void SyncEngine::DoEnableApp(const std::string& app_id,
                             const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void SyncEngine::DidInitialize(SyncEngineInitializer* initializer,
                               SyncStatusCode status) {
  metadata_database_ = initializer->PassMetadataDatabase();
}

void SyncEngine::DidProcessRemoteChange(RemoteToLocalSyncer* syncer,
                                        const SyncFileCallback& callback,
                                        SyncStatusCode status) {
  NOTIMPLEMENTED();
}

void SyncEngine::DidApplyLocalChange(LocalToRemoteSyncer* syncer,
                                     const SyncStatusCallback& callback,
                                     SyncStatusCode status) {
  NOTIMPLEMENTED();
}

}  // namespace drive_backend
}  // namespace sync_file_system
