// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_system_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/download_handler.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_file_system.h"
#include "chrome/browser/chromeos/drive/drive_webapps_registry.h"
#include "chrome/browser/chromeos/drive/file_system_proxy.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/file_write_helper.h"
#include "chrome/browser/chromeos/drive/logging.h"
#include "chrome/browser/chromeos/drive/stale_cache_files_remover.h"
#include "chrome/browser/chromeos/drive/sync_client.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/drive_api_service.h"
#include "chrome/browser/google_apis/drive_api_util.h"
#include "chrome/browser/google_apis/drive_notification_manager.h"
#include "chrome/browser/google_apis/drive_notification_manager_factory.h"
#include "chrome/browser/google_apis/gdata_wapi_service.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google/cacheinvalidation/types.pb.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/user_agent/user_agent_util.h"

using content::BrowserContext;
using content::BrowserThread;

namespace drive {
namespace {

static const size_t kEventLogHistorySize = 100;

// Returns true if Drive is enabled for the given Profile.
bool IsDriveEnabledForProfile(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!google_apis::AuthService::CanAuthenticate(profile))
    return false;

  // Disable Drive if preference is set.  This can happen with commandline flag
  // --disable-gdata or enterprise policy, or probably with user settings too
  // in the future.
  if (profile->GetPrefs()->GetBoolean(prefs::kDisableDrive))
    return false;

  return true;
}

// Returns a user agent string used for communicating with the Drive backend,
// both WAPI and Drive API.  The user agent looks like:
//
// chromedrive-<VERSION> chrome-cc/none (<OS_CPU_INFO>)
// chromedrive-24.0.1274.0 chrome-cc/none (CrOS x86_64 0.4.0)
//
// TODO(satorux): Move this function to somewhere else: crbug.com/151605
std::string GetDriveUserAgent() {
  const char kDriveClientName[] = "chromedrive";

  chrome::VersionInfo version_info;
  const std::string version = (version_info.is_valid() ?
                               version_info.Version() :
                               std::string("unknown"));

  // This part is <client_name>/<version>.
  const char kLibraryInfo[] = "chrome-cc/none";

  const std::string os_cpu_info = webkit_glue::BuildOSCpuInfo();

  // Add "gzip" to receive compressed data from the server.
  // (see https://developers.google.com/drive/performance)
  return base::StringPrintf("%s-%s %s (%s) (gzip)",
                            kDriveClientName,
                            version.c_str(),
                            kLibraryInfo,
                            os_cpu_info.c_str());
}

}  // namespace

DriveSystemService::DriveSystemService(
    Profile* profile,
    google_apis::DriveServiceInterface* test_drive_service,
    const base::FilePath& test_cache_root,
    DriveFileSystemInterface* test_file_system)
    : profile_(profile),
      drive_disabled_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::SequencedWorkerPool* blocking_pool = BrowserThread::GetBlockingPool();
  blocking_task_runner_ = blocking_pool->GetSequencedTaskRunner(
      blocking_pool->GetSequenceToken());

  if (test_drive_service) {
    drive_service_.reset(test_drive_service);
  } else if (google_apis::util::IsDriveV2ApiEnabled()) {
    drive_service_.reset(new google_apis::DriveAPIService(
        g_browser_process->system_request_context(),
        GURL(google_apis::DriveApiUrlGenerator::kBaseUrlForProduction),
        GURL(google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction),
        GetDriveUserAgent()));
  } else {
    drive_service_.reset(new google_apis::GDataWapiService(
        g_browser_process->system_request_context(),
        GURL(google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction),
        GetDriveUserAgent()));
  }
  scheduler_.reset(new JobScheduler(profile_, drive_service_.get()));
  cache_.reset(new DriveCache(!test_cache_root.empty() ? test_cache_root :
                              DriveCache::GetCacheRootPath(profile),
                              blocking_task_runner_,
                              NULL /* free_disk_space_getter */));
  webapps_registry_.reset(new DriveWebAppsRegistry);

  // We can call DriveCache::GetCacheDirectoryPath safely even before the cache
  // gets initialized.
  resource_metadata_.reset(new internal::ResourceMetadata(
      cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_META),
      blocking_task_runner_));

  file_system_.reset(test_file_system ? test_file_system :
                     new DriveFileSystem(profile_,
                                         cache(),
                                         drive_service_.get(),
                                         scheduler_.get(),
                                         webapps_registry(),
                                         resource_metadata_.get(),
                                         blocking_task_runner_));
  file_write_helper_.reset(new FileWriteHelper(file_system()));
  download_handler_.reset(new DownloadHandler(file_write_helper(),
                                              file_system()));
  sync_client_.reset(new SyncClient(file_system(), cache()));
  stale_cache_files_remover_.reset(new StaleCacheFilesRemover(file_system(),
                                                              cache()));
}

DriveSystemService::~DriveSystemService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void DriveSystemService::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  drive_service_->Initialize(profile_);
  file_system_->Initialize();
  cache_->RequestInitialize(
      base::Bind(&DriveSystemService::InitializeAfterCacheInitialized,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DriveSystemService::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  google_apis::DriveNotificationManager* drive_notification_manager =
      google_apis::DriveNotificationManagerFactory::GetForProfile(profile_);
  if (drive_notification_manager)
    drive_notification_manager->RemoveObserver(this);

  RemoveDriveMountPoint();
}

void DriveSystemService::AddObserver(DriveSystemServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void DriveSystemService::RemoveObserver(DriveSystemServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void DriveSystemService::OnNotificationReceived() {
  file_system_->CheckForUpdates();
}

bool DriveSystemService::IsDriveEnabled() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!IsDriveEnabledForProfile(profile_))
    return false;

  // Drive may be disabled for cache initialization failure, etc.
  if (drive_disabled_)
    return false;

  return true;
}

void DriveSystemService::ClearCacheAndRemountFileSystem(
    const base::Callback<void(bool)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  RemoveDriveMountPoint();
  cache_->ClearAll(base::Bind(
      &DriveSystemService::ReinitializeResourceMetadataAfterClearCache,
      weak_ptr_factory_.GetWeakPtr(),
      callback));
}

void DriveSystemService::ReinitializeResourceMetadataAfterClearCache(
    const base::Callback<void(bool)>& callback,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!success) {
    callback.Run(false);
    return;
  }
  resource_metadata_->Initialize(
      base::Bind(&DriveSystemService::AddBackDriveMountPoint,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void DriveSystemService::AddBackDriveMountPoint(
    const base::Callback<void(bool)>& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(false);
    return;
  }
  file_system_->Initialize();
  AddDriveMountPoint();

  callback.Run(true);
}

void DriveSystemService::ReloadAndRemountFileSystem() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RemoveDriveMountPoint();
  file_system_->Reload();

  // Reload() is asynchronous. But we can add back the mount point right away
  // because every operation waits until loading is complete.
  AddDriveMountPoint();
}

void DriveSystemService::AddDriveMountPoint() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!file_system_proxy_.get());

  const base::FilePath drive_mount_point = util::GetDriveMountPointPath();
  fileapi::ExternalMountPoints* mount_points =
      BrowserContext::GetMountPoints(profile_);
  DCHECK(mount_points);

  file_system_proxy_ = new FileSystemProxy(file_system_.get());

  bool success = mount_points->RegisterRemoteFileSystem(
      drive_mount_point.BaseName().AsUTF8Unsafe(),
      fileapi::kFileSystemTypeDrive,
      file_system_proxy_,
      drive_mount_point);

  if (success) {
    util::Log("Drive mount point is added");
    FOR_EACH_OBSERVER(DriveSystemServiceObserver, observers_,
                      OnFileSystemMounted());
  }
}

void DriveSystemService::RemoveDriveMountPoint() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  job_list()->CancelAllJobs();

  FOR_EACH_OBSERVER(DriveSystemServiceObserver, observers_,
                    OnFileSystemBeingUnmounted());

  fileapi::ExternalMountPoints* mount_points =
      BrowserContext::GetMountPoints(profile_);
  DCHECK(mount_points);

  mount_points->RevokeFileSystem(
      util::GetDriveMountPointPath().BaseName().AsUTF8Unsafe());
  if (file_system_proxy_) {
    file_system_proxy_->DetachFromFileSystem();
    file_system_proxy_ = NULL;
  }
  util::Log("Drive mount point is removed");
}

void DriveSystemService::InitializeAfterCacheInitialized(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!success) {
    LOG(WARNING) << "Failed to initialize the cache. Disabling Drive";
    DisableDrive();
    return;
  }

  resource_metadata_->Initialize(
      base::Bind(
          &DriveSystemService::InitializeAfterResourceMetadataInitialized,
          weak_ptr_factory_.GetWeakPtr()));
}

void DriveSystemService::InitializeAfterResourceMetadataInitialized(
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != FILE_ERROR_OK) {
    LOG(WARNING) << "Failed to initialize resource metadata. Disabling Drive : "
                 << FileErrorToString(error);
    DisableDrive();
    return;
  }

  content::DownloadManager* download_manager =
    g_browser_process->download_status_updater() ?
        BrowserContext::GetDownloadManager(profile_) : NULL;
  download_handler_->Initialize(
      download_manager,
      cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_TMP_DOWNLOADS));

  // Register for Google Drive invalidation notifications.
  google_apis::DriveNotificationManager* drive_notification_manager =
      google_apis::DriveNotificationManagerFactory::GetForProfile(profile_);
  if (drive_notification_manager)
    drive_notification_manager->AddObserver(this);

  AddDriveMountPoint();
}

void DriveSystemService::DisableDrive() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drive_disabled_ = true;
  // Change the download directory to the default value if the download
  // destination is set to under Drive mount point.
  PrefService* pref_service = profile_->GetPrefs();
  if (util::IsUnderDriveMountPoint(
          pref_service->GetFilePath(prefs::kDownloadDefaultDirectory))) {
    pref_service->SetFilePath(prefs::kDownloadDefaultDirectory,
                              download_util::GetDefaultDownloadDirectory());
  }
}

//===================== DriveSystemServiceFactory =============================

// static
DriveSystemService* DriveSystemServiceFactory::GetForProfile(
    Profile* profile) {
  DriveSystemService* service = GetForProfileRegardlessOfStates(profile);
  if (service && !service->IsDriveEnabled())
    return NULL;

  return service;
}

// static
DriveSystemService* DriveSystemServiceFactory::GetForProfileRegardlessOfStates(
    Profile* profile) {
  return static_cast<DriveSystemService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
DriveSystemService* DriveSystemServiceFactory::FindForProfile(
    Profile* profile) {
  DriveSystemService* service = FindForProfileRegardlessOfStates(profile);
  if (service && !service->IsDriveEnabled())
    return NULL;

  return service;
}

// static
DriveSystemService* DriveSystemServiceFactory::FindForProfileRegardlessOfStates(
    Profile* profile) {
  return static_cast<DriveSystemService*>(
      GetInstance()->GetServiceForProfile(profile, false));
}

// static
DriveSystemServiceFactory* DriveSystemServiceFactory::GetInstance() {
  return Singleton<DriveSystemServiceFactory>::get();
}

// static
void DriveSystemServiceFactory::SetFactoryForTest(
    const FactoryCallback& factory_for_test) {
  GetInstance()->factory_for_test_ = factory_for_test;
}

DriveSystemServiceFactory::DriveSystemServiceFactory()
    : ProfileKeyedServiceFactory("DriveSystemService",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(google_apis::DriveNotificationManagerFactory::GetInstance());
  DependsOn(DownloadServiceFactory::GetInstance());
}

DriveSystemServiceFactory::~DriveSystemServiceFactory() {
}

ProfileKeyedService* DriveSystemServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  DriveSystemService* service = NULL;
  if (factory_for_test_.is_null())
    service = new DriveSystemService(profile, NULL, base::FilePath(), NULL);
  else
    service = factory_for_test_.Run(profile);

  service->Initialize();
  return service;
}

}  // namespace drive
