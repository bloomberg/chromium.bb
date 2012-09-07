// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_system_service.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/gdata/drive_api_service.h"
#include "chrome/browser/chromeos/gdata/drive_download_observer.h"
#include "chrome/browser/chromeos/gdata/drive_file_system.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_proxy.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/gdata/drive_sync_client.h"
#include "chrome/browser/chromeos/gdata/drive_uploader.h"
#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"
#include "chrome/browser/chromeos/gdata/file_write_helper.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_service.h"
#include "chrome/browser/chromeos/gdata/stale_cache_files_remover.h"
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

DriveSystemService::DriveSystemService(Profile* profile)
    : profile_(profile),
      cache_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::SequencedWorkerPool* blocking_pool = BrowserThread::GetBlockingPool();
  blocking_task_runner_ = blocking_pool->GetSequencedTaskRunner(
      blocking_pool->GetSequenceToken());
}

DriveSystemService::~DriveSystemService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cache_->DestroyOnUIThread();
}

void DriveSystemService::Initialize(
    DriveServiceInterface* drive_service,
    const FilePath& cache_root) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drive_service_.reset(drive_service);
  cache_ = DriveCache::CreateDriveCacheOnUIThread(
      cache_root,
      blocking_task_runner_);
  uploader_.reset(new DriveUploader(drive_service_.get()));
  webapps_registry_.reset(new DriveWebAppsRegistry);
  file_system_.reset(new DriveFileSystem(profile_,
                                         cache(),
                                         drive_service_.get(),
                                         uploader(),
                                         webapps_registry(),
                                         blocking_task_runner_));
  file_write_helper_.reset(new FileWriteHelper(file_system()));
  download_observer_.reset(new DriveDownloadObserver(uploader(),
                                                     file_system()));
  sync_client_.reset(new DriveSyncClient(profile_, file_system(), cache()));
  stale_cache_files_remover_.reset(new StaleCacheFilesRemover(file_system(),
                                                              cache()));

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
  file_system_->StartInitialFeedFetch();
}

void DriveSystemService::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RemoveDriveMountPoint();

  // Shut down the member objects in the reverse order of creation.
  stale_cache_files_remover_.reset();
  sync_client_.reset();
  download_observer_.reset();
  file_write_helper_.reset();
  file_system_.reset();
  webapps_registry_.reset();
  uploader_.reset();
  drive_service_.reset();
}

void DriveSystemService::ClearCacheAndRemountFileSystem(
    const base::Callback<void(bool)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RemoveDriveMountPoint();
  drive_service()->CancelAll();
  cache_->ClearAllOnUIThread(
      base::Bind(&DriveSystemService::AddBackDriveMountPoint,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void DriveSystemService::AddBackDriveMountPoint(
    const base::Callback<void(bool)>& callback,
    DriveFileError error,
    const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_system_->Initialize();
  AddDriveMountPoint();

  if (!callback.is_null())
    callback.Run(error == DRIVE_FILE_OK);
}

void DriveSystemService::AddDriveMountPoint() {
  if (!gdata::util::IsGDataAvailable(profile_))
    return;

  const FilePath mount_point = gdata::util::GetDriveMountPointPath();
  fileapi::ExternalFileSystemMountPointProvider* provider =
      BrowserContext::GetFileSystemContext(profile_)->external_provider();
  if (provider && !provider->HasMountPoint(mount_point)) {
    provider->AddRemoteMountPoint(
        mount_point,
        new DriveFileSystemProxy(file_system_.get()));
  }

  file_system_->NotifyFileSystemMounted();
}

void DriveSystemService::RemoveDriveMountPoint() {
  file_system_->NotifyFileSystemToBeUnmounted();
  file_system_->StopUpdates();

  const FilePath mount_point = gdata::util::GetDriveMountPointPath();
  fileapi::ExternalFileSystemMountPointProvider* provider =
      BrowserContext::GetFileSystemContext(profile_)->external_provider();
  if (provider && provider->HasMountPoint(mount_point))
    provider->RemoveMountPoint(mount_point);
}

//===================== DriveSystemServiceFactory =============================

// static
DriveSystemService* DriveSystemServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DriveSystemService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
DriveSystemService* DriveSystemServiceFactory::FindForProfile(
    Profile* profile) {
  return static_cast<DriveSystemService*>(
      GetInstance()->GetServiceForProfile(profile, false));
}

// static
DriveSystemServiceFactory* DriveSystemServiceFactory::GetInstance() {
  return Singleton<DriveSystemServiceFactory>::get();
}

DriveSystemServiceFactory::DriveSystemServiceFactory()
    : ProfileKeyedServiceFactory("DriveSystemService",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(DownloadServiceFactory::GetInstance());
}

DriveSystemServiceFactory::~DriveSystemServiceFactory() {
}

// static
void DriveSystemServiceFactory::set_drive_service_for_test(
    DriveServiceInterface* drive_service) {
  if (g_test_drive_service)
    delete g_test_drive_service;
  g_test_drive_service = drive_service;
}

// static
void DriveSystemServiceFactory::set_cache_root_for_test(
    const std::string& cache_root) {
  if (g_test_cache_root)
    delete g_test_cache_root;
  g_test_cache_root = !cache_root.empty() ? new std::string(cache_root) : NULL;
}

ProfileKeyedService* DriveSystemServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  DriveSystemService* service = new DriveSystemService(profile);

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
