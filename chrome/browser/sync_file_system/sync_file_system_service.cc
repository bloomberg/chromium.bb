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
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

using content::BrowserThread;

namespace sync_file_system {

void SyncFileSystemService::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  local_file_service_->Shutdown();
  local_file_service_.reset();

  profile_ = NULL;
}

SyncFileSystemService::~SyncFileSystemService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!profile_);
}

void SyncFileSystemService::InitializeForApp(
    fileapi::FileSystemContext* file_system_context,
    const std::string& service_name,
    const GURL& app_url,
    const StatusCallback& callback) {
  DCHECK(local_file_service_.get());

  // TODO(kinuko,tzik): Instantiate the remote_file_service for the given
  // |service_name| if it hasn't been initialized.

  local_file_service_->MaybeInitializeFileSystemContext(
      app_url, file_system_context, callback);

  // TODO(tzik): Hook up remote service initialization code.
}

SyncFileSystemService::SyncFileSystemService(Profile* profile)
    : profile_(profile) {}

void SyncFileSystemService::Initialize(
    scoped_ptr<LocalFileSyncService> local_file_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(local_file_service.get());
  DCHECK(profile_);

  local_file_service_ = local_file_service.Pass();
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

SyncFileSystemServiceFactory::SyncFileSystemServiceFactory()
    : ProfileKeyedServiceFactory("SyncFileSystemService",
                                 ProfileDependencyManager::GetInstance()) {
}

SyncFileSystemServiceFactory::~SyncFileSystemServiceFactory() {}

ProfileKeyedService* SyncFileSystemServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  SyncFileSystemService* service = new SyncFileSystemService(profile);

  // TODO(kinuko): Set up mock services if it is called for testing.

  scoped_ptr<LocalFileSyncService> local_file_service(
      new LocalFileSyncService);

  service->Initialize(local_file_service.Pass());
  return service;
}

}  // namespace chrome
