// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_system_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"
#include "chrome/browser/chromeos/gdata/gdata_documents_service.h"
#include "chrome/browser/chromeos/gdata/gdata_download_observer.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system_proxy.h"
#include "chrome/browser/chromeos/gdata/gdata_sync_client.h"
#include "chrome/browser/chromeos/gdata/gdata_uploader.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

using content::BrowserContext;
using content::BrowserThread;

namespace {

scoped_refptr<base::SequencedTaskRunner> GetTaskRunner(
    const base::SequencedWorkerPool::SequenceToken& sequence_token) {
  return BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
      sequence_token);
}

}  // nemaspace

namespace gdata {

//===================== GDataSystemService ====================================
GDataSystemService::GDataSystemService(Profile* profile)
    : profile_(profile),
      sequence_token_(BrowserThread::GetBlockingPool()->GetSequenceToken()),
      cache_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataSystemService::~GDataSystemService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cache_->DestroyOnUIThread();
}

void GDataSystemService::Initialize(
    DocumentsServiceInterface* documents_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  documents_service_.reset(documents_service);
  cache_ = GDataCache::CreateGDataCacheOnUIThread(
      GDataCache::GetCacheRootPath(profile_),
      GetTaskRunner(sequence_token_));
  uploader_.reset(new GDataUploader(docs_service()));
  webapps_registry_.reset(new DriveWebAppsRegistry);
  file_system_.reset(new GDataFileSystem(profile_,
                                         cache(),
                                         docs_service(),
                                         uploader(),
                                         webapps_registry(),
                                         GetTaskRunner(sequence_token_)));
  download_observer_.reset(new GDataDownloadObserver(uploader(),
                                                     file_system()));
  sync_client_.reset(new GDataSyncClient(profile_, file_system(), cache()));

  sync_client_->Initialize();
  file_system_->Initialize();
  cache_->RequestInitializeOnUIThread();

  content::DownloadManager* download_manager =
    g_browser_process->download_status_updater() ?
        BrowserContext::GetDownloadManager(profile_) : NULL;
  download_observer_->Initialize(
      download_manager,
      cache_->GetCacheDirectoryPath(
          GDataCache::CACHE_TYPE_TMP_DOWNLOADS));

  AddDriveMountPoint();
}

void GDataSystemService::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RemoveDriveMountPoint();

  // Shut down the member objects in the reverse order of creation.
  sync_client_.reset();
  download_observer_.reset();
  file_system_.reset();
  webapps_registry_.reset();
  uploader_.reset();
  documents_service_.reset();
}

void GDataSystemService::AddDriveMountPoint() {
  if (!gdata::util::IsGDataAvailable(profile_))
    return;

  const FilePath mount_point = gdata::util::GetGDataMountPointPath();
  fileapi::ExternalFileSystemMountPointProvider* provider =
      BrowserContext::GetFileSystemContext(profile_)->external_provider();
  if (provider && !provider->HasMountPoint(mount_point)) {
    provider->AddRemoteMountPoint(
        mount_point,
        new GDataFileSystemProxy(file_system_.get()));
  }
}

void GDataSystemService::RemoveDriveMountPoint() {
  const FilePath mount_point = gdata::util::GetGDataMountPointPath();
  fileapi::ExternalFileSystemMountPointProvider* provider =
      BrowserContext::GetFileSystemContext(profile_)->external_provider();
  if (provider && provider->HasMountPoint(mount_point))
    provider->RemoveMountPoint(mount_point);
}

//===================== GDataSystemServiceFactory =============================

// static
GDataSystemService* GDataSystemServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GDataSystemService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
GDataSystemService* GDataSystemServiceFactory::FindForProfile(
    Profile* profile) {
  return static_cast<GDataSystemService*>(
      GetInstance()->GetServiceForProfile(profile, false));
}

// static
GDataSystemServiceFactory* GDataSystemServiceFactory::GetInstance() {
  return Singleton<GDataSystemServiceFactory>::get();
}

GDataSystemServiceFactory::GDataSystemServiceFactory()
    : ProfileKeyedServiceFactory("GDataSystemService",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(DownloadServiceFactory::GetInstance());
}

GDataSystemServiceFactory::~GDataSystemServiceFactory() {
}

// static
ProfileKeyedService* GDataSystemServiceFactory::CreateInstance(
    Profile* profile) {
  return new GDataSystemService(profile);
}

GDataSystemService*
GDataSystemServiceFactory::GetWithCustomDocumentsServiceForTesting(
    Profile* profile,
    DocumentsServiceInterface* documents_service) {
  GDataSystemService* service =
      static_cast<GDataSystemService*>(GetInstance()->SetTestingFactoryAndUse(
          profile,
          &GDataSystemServiceFactory::CreateInstance));
  service->Initialize(documents_service);
  return service;
}

ProfileKeyedService* GDataSystemServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  GDataSystemService* service = new GDataSystemService(profile);
  service->Initialize(new DocumentsService);
  return service;
}

}  // namespace gdata
