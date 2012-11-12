// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/sync_file_system/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

using content::BrowserThread;
using fileapi::ConflictFileInfoCallback;
using fileapi::FileSystemURL;
using fileapi::SyncFileMetadata;
using fileapi::SyncStatusCallback;
using fileapi::SyncStatusCode;

namespace sync_file_system {

void SyncFileSystemService::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  local_file_service_->Shutdown();
  local_file_service_.reset();

  remote_file_service_.reset();

  profile_ = NULL;
}

SyncFileSystemService::~SyncFileSystemService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!profile_);
}

void SyncFileSystemService::InitializeForApp(
    fileapi::FileSystemContext* file_system_context,
    const std::string& service_name,
    const GURL& app_origin,
    const SyncStatusCallback& callback) {
  DCHECK(local_file_service_);
  DCHECK(app_origin == app_origin.GetOrigin());

  // TODO(kinuko,tzik): Instantiate the remote_file_service for the given
  // |service_name| if it hasn't been initialized.

  local_file_service_->MaybeInitializeFileSystemContext(
      app_origin, service_name, file_system_context, callback);

  if (remote_file_service_) {
    // TODO(tzik): Handle errors in the completion callback.
    remote_file_service_->RegisterOriginForTrackingChanges(
        app_origin, fileapi::SyncStatusCallback());
  }
}

void SyncFileSystemService::GetConflictFiles(
    const GURL& app_origin,
    const std::string& service_name,
    const fileapi::SyncFileSetCallback& callback) {
  DCHECK(app_origin == app_origin.GetOrigin());

  // TODO(kinuko): Implement.
  NOTIMPLEMENTED();
}

void SyncFileSystemService::GetConflictFileInfo(
    const GURL& app_origin,
    const std::string& service_name,
    const FileSystemURL& url,
    const ConflictFileInfoCallback& callback) {
  DCHECK(local_file_service_);
  DCHECK(app_origin == app_origin.GetOrigin());

  // TODO(kinuko): Implement.
  NOTIMPLEMENTED();
}

void SyncFileSystemService::OnLocalChangeAvailable(int64 pending_changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_GE(pending_changes, 0);
  pending_local_changes_ = pending_changes;
}

void SyncFileSystemService::OnRemoteChangeAvailable(int64 pending_changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_GE(pending_changes, 0);
  pending_remote_changes_ = pending_changes;
}

SyncFileSystemService::SyncFileSystemService(Profile* profile)
    : profile_(profile),
      pending_local_changes_(0),
      pending_remote_changes_(0) {}

void SyncFileSystemService::Initialize(
    scoped_ptr<LocalFileSyncService> local_file_service,
    scoped_ptr<RemoteFileSyncService> remote_file_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(local_file_service);
  DCHECK(profile_);

  local_file_service_ = local_file_service.Pass();
  remote_file_service_ = remote_file_service.Pass();

  if (remote_file_service_)
    remote_file_service_->AddObserver(this);
}

// SyncFileSystemServiceFactory -----------------------------------------------

// static
SyncFileSystemService* SyncFileSystemServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SyncFileSystemService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
SyncFileSystemServiceFactory* SyncFileSystemServiceFactory::GetInstance() {
  return Singleton<SyncFileSystemServiceFactory>::get();
}

void SyncFileSystemServiceFactory::set_mock_remote_file_service(
    scoped_ptr<RemoteFileSyncService> mock_remote_service) {
  mock_remote_file_service_ = mock_remote_service.Pass();
}

SyncFileSystemServiceFactory::SyncFileSystemServiceFactory()
    : ProfileKeyedServiceFactory("SyncFileSystemService",
                                 ProfileDependencyManager::GetInstance()) {
}

SyncFileSystemServiceFactory::~SyncFileSystemServiceFactory() {}

ProfileKeyedService* SyncFileSystemServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  SyncFileSystemService* service = new SyncFileSystemService(profile);

  scoped_ptr<LocalFileSyncService> local_file_service(
      new LocalFileSyncService);

  scoped_ptr<RemoteFileSyncService> remote_file_service;
  // TODO(tzik): Instantiate DriveFileSyncService.

  if (mock_remote_file_service_)
    remote_file_service = mock_remote_file_service_.Pass();

  service->Initialize(local_file_service.Pass(),
                      remote_file_service.Pass());
  return service;
}

}  // namespace sync_file_system
