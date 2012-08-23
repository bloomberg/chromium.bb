// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_system_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/gdata/drive_api_service.h"
#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"
#include "chrome/browser/chromeos/gdata/file_write_helper.h"
#include "chrome/browser/chromeos/gdata/gdata_contacts_service.h"
#include "chrome/browser/chromeos/gdata/gdata_download_observer.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system_proxy.h"
#include "chrome/browser/chromeos/gdata/gdata_sync_client.h"
#include "chrome/browser/chromeos/gdata/gdata_uploader.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_service.h"
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

namespace gdata {
namespace {

// Used in test to setup system service.
DriveServiceInterface* g_test_drive_service = NULL;
const std::string* g_test_cache_root = NULL;

}  // namespace

GDataSystemService::GDataSystemService(Profile* profile)
    : profile_(profile),
      cache_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::SequencedWorkerPool* blocking_pool = BrowserThread::GetBlockingPool();
  blocking_task_runner_ = blocking_pool->GetSequencedTaskRunner(
      blocking_pool->GetSequenceToken());
}

GDataSystemService::~GDataSystemService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cache_->DestroyOnUIThread();
}

void GDataSystemService::Initialize(
    DriveServiceInterface* drive_service,
    const FilePath& cache_root) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drive_service_.reset(drive_service);
  cache_ = DriveCache::CreateDriveCacheOnUIThread(
      cache_root,
      blocking_task_runner_);
  uploader_.reset(new GDataUploader(drive_service_.get()));
  webapps_registry_.reset(new DriveWebAppsRegistry);
  file_system_.reset(new GDataFileSystem(profile_,
                                         cache(),
                                         drive_service_.get(),
                                         uploader(),
                                         webapps_registry(),
                                         blocking_task_runner_));
  file_write_helper_.reset(new FileWriteHelper(file_system()));
  download_observer_.reset(new GDataDownloadObserver(uploader(),
                                                     file_system()));
  sync_client_.reset(new GDataSyncClient(profile_, file_system(), cache()));
  contacts_service_.reset(new GDataContactsService(profile_));

  sync_client_->Initialize();
  file_system_->Initialize();
  cache_->RequestInitializeOnUIThread();

  content::DownloadManager* download_manager =
    g_browser_process->download_status_updater() ?
        BrowserContext::GetDownloadManager(profile_) : NULL;
  download_observer_->Initialize(
      download_manager,
      cache_->GetCacheDirectoryPath(
          DriveCache::CACHE_TYPE_TMP_DOWNLOADS));

  AddDriveMountPoint();
  contacts_service_->Initialize();
}

void GDataSystemService::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RemoveDriveMountPoint();

  // Shut down the member objects in the reverse order of creation.
  contacts_service_.reset();
  sync_client_.reset();
  download_observer_.reset();
  file_write_helper_.reset();
  file_system_.reset();
  webapps_registry_.reset();
  uploader_.reset();
  drive_service_.reset();
}

void GDataSystemService::ClearCacheAndRemountFileSystem(
    const base::Callback<void(bool)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RemoveDriveMountPoint();
  drive_service()->CancelAll();
  cache_->ClearAllOnUIThread(
      base::Bind(&GDataSystemService::AddBackDriveMountPoint,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void GDataSystemService::AddBackDriveMountPoint(
    const base::Callback<void(bool)>& callback,
    DriveFileError error,
    const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  AddDriveMountPoint();

  if (!callback.is_null())
    callback.Run(error == DRIVE_FILE_OK);
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

  file_system_->Initialize();
  file_system_->NotifyFileSystemMounted();
}

void GDataSystemService::RemoveDriveMountPoint() {
  file_system_->NotifyFileSystemToBeUnmounted();
  file_system_->StopUpdates();

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
void GDataSystemServiceFactory::set_drive_service_for_test(
    DriveServiceInterface* drive_service) {
  if (g_test_drive_service)
    delete g_test_drive_service;
  g_test_drive_service = drive_service;
}

// static
void GDataSystemServiceFactory::set_cache_root_for_test(
    const std::string& cache_root) {
  if (g_test_cache_root)
    delete g_test_cache_root;
  g_test_cache_root = !cache_root.empty() ? new std::string(cache_root) : NULL;
}

ProfileKeyedService* GDataSystemServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  GDataSystemService* service = new GDataSystemService(profile);

  DriveServiceInterface* drive_service = g_test_drive_service;
  g_test_drive_service = NULL;
  if (!drive_service) {
    if (util::IsDriveV2ApiEnabled())
      drive_service = new DriveAPIService();
    else
      drive_service = new GDataWapiService();
  }

  FilePath cache_root =
      g_test_cache_root ? FilePath(*g_test_cache_root) :
                          DriveCache::GetCacheRootPath(profile);
  delete g_test_cache_root;
  g_test_cache_root = NULL;

  service->Initialize(drive_service, cache_root);
  return service;
}

}  // namespace gdata
