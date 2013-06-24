// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_integration_service.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/prefs/pref_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/debug_info_collector.h"
#include "chrome/browser/chromeos/drive/download_handler.h"
#include "chrome/browser/chromeos/drive/drive_app_registry.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system.h"
#include "chrome/browser/chromeos/drive/file_system_proxy.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/file_write_helper.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/logging.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/drive/drive_api_service.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_notification_manager.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/drive/gdata_wapi_service.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/common/user_agent/user_agent_util.h"

using content::BrowserContext;
using content::BrowserThread;

namespace drive {
namespace {

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

// Initializes FileCache and ResourceMetadata.
// Must be run on the same task runner used by |cache| and |resource_metadata|.
FileError InitializeMetadata(
    const base::FilePath& cache_root_directory,
    internal::ResourceMetadataStorage* metadata_storage,
    internal::FileCache* cache,
    internal::ResourceMetadata* resource_metadata) {
  if (!file_util::CreateDirectory(cache_root_directory.Append(
          util::kMetadataDirectory)) ||
      !file_util::CreateDirectory(cache_root_directory.Append(
          util::kCacheFileDirectory)) ||
      !file_util::CreateDirectory(cache_root_directory.Append(
          util::kTemporaryFileDirectory))) {
    LOG(WARNING) << "Failed to create directories.";
    return FILE_ERROR_FAILED;
  }

  // Change permissions of cache file directory to u+rwx,og+x (711) in order to
  // allow archive files in that directory to be mounted by cros-disks.
  file_util::SetPosixFilePermissions(
      cache_root_directory.Append(util::kCacheFileDirectory),
      file_util::FILE_PERMISSION_USER_MASK |
      file_util::FILE_PERMISSION_EXECUTE_BY_GROUP |
      file_util::FILE_PERMISSION_EXECUTE_BY_OTHERS);

  util::MigrateCacheFilesFromOldDirectories(cache_root_directory);

  if (!metadata_storage->Initialize()) {
    LOG(WARNING) << "Failed to initialize the metadata storage.";
    return FILE_ERROR_FAILED;
  }

  if (!cache->Initialize()) {
    LOG(WARNING) << "Failed to initialize the cache.";
    return FILE_ERROR_FAILED;
  }

  FileError error = resource_metadata->Initialize();
  LOG_IF(WARNING, error != FILE_ERROR_OK)
      << "Failed to initialize resource metadata. " << FileErrorToString(error);
  return error;
}

}  // namespace

DriveIntegrationService::DriveIntegrationService(
    Profile* profile,
    DriveServiceInterface* test_drive_service,
    const base::FilePath& test_cache_root,
    FileSystemInterface* test_file_system)
    : profile_(profile),
      drive_disabled_(false),
      cache_root_directory_(!test_cache_root.empty() ?
                            test_cache_root : util::GetCacheRootPath(profile)),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::SequencedWorkerPool* blocking_pool = BrowserThread::GetBlockingPool();
  blocking_task_runner_ = blocking_pool->GetSequencedTaskRunner(
      blocking_pool->GetSequenceToken());

  if (test_drive_service) {
    drive_service_.reset(test_drive_service);
  } else if (util::IsDriveV2ApiEnabled()) {
    drive_service_.reset(new DriveAPIService(
        g_browser_process->system_request_context(),
        GURL(google_apis::DriveApiUrlGenerator::kBaseUrlForProduction),
        GetDriveUserAgent()));
  } else {
    drive_service_.reset(new GDataWapiService(
        g_browser_process->system_request_context(),
        GURL(google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction),
        GetDriveUserAgent()));
  }
  scheduler_.reset(new JobScheduler(
      profile_, drive_service_.get(), blocking_task_runner_.get()));
  metadata_storage_.reset(new internal::ResourceMetadataStorage(
      cache_root_directory_.Append(util::kMetadataDirectory),
      blocking_task_runner_.get()));
  cache_.reset(new internal::FileCache(
      metadata_storage_.get(),
      cache_root_directory_.Append(util::kCacheFileDirectory),
      blocking_task_runner_.get(),
      NULL /* free_disk_space_getter */));
  drive_app_registry_.reset(new DriveAppRegistry(scheduler_.get()));

  resource_metadata_.reset(new internal::ResourceMetadata(
      metadata_storage_.get(), blocking_task_runner_));

  file_system_.reset(
      test_file_system ? test_file_system : new FileSystem(
          profile_,
          cache_.get(),
          drive_service_.get(),
          scheduler_.get(),
          resource_metadata_.get(),
          blocking_task_runner_.get(),
          cache_root_directory_.Append(util::kTemporaryFileDirectory)));
  file_write_helper_.reset(new FileWriteHelper(file_system()));
  download_handler_.reset(new DownloadHandler(file_write_helper(),
                                              file_system()));
  debug_info_collector_.reset(
      new DebugInfoCollector(file_system(), cache_.get()));
}

DriveIntegrationService::~DriveIntegrationService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void DriveIntegrationService::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  drive_service_->Initialize(profile_);
  file_system_->Initialize();

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&InitializeMetadata,
                 cache_root_directory_,
                 metadata_storage_.get(),
                 cache_.get(),
                 resource_metadata_.get()),
      base::Bind(&DriveIntegrationService::InitializeAfterMetadataInitialized,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DriveIntegrationService::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveNotificationManager* drive_notification_manager =
      DriveNotificationManagerFactory::GetForProfile(profile_);
  if (drive_notification_manager)
    drive_notification_manager->RemoveObserver(this);

  RemoveDriveMountPoint();
}

void DriveIntegrationService::AddObserver(
    DriveIntegrationServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void DriveIntegrationService::RemoveObserver(
    DriveIntegrationServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void DriveIntegrationService::OnNotificationReceived() {
  file_system_->CheckForUpdates();
  drive_app_registry_->Update();
}

void DriveIntegrationService::OnPushNotificationEnabled(bool enabled) {
  const char* status = (enabled ? "enabled" : "disabled");
  util::Log("Push notification is %s", status);
}

bool DriveIntegrationService::IsDriveEnabled() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!IsDriveEnabledForProfile(profile_))
    return false;

  // Drive may be disabled for cache initialization failure, etc.
  if (drive_disabled_)
    return false;

  return true;
}

void DriveIntegrationService::ClearCacheAndRemountFileSystem(
    const base::Callback<void(bool)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  RemoveDriveMountPoint();
  cache_->ClearAllOnUIThread(base::Bind(
      &DriveIntegrationService::AddBackDriveMountPoint,
      weak_ptr_factory_.GetWeakPtr(),
      callback));
}

void DriveIntegrationService::AddBackDriveMountPoint(
    const base::Callback<void(bool)>& callback,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!success) {
    callback.Run(false);
    return;
  }

  file_system_->Initialize();
  drive_app_registry_->Update();
  AddDriveMountPoint();

  callback.Run(true);
}

void DriveIntegrationService::ReloadAndRemountFileSystem() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RemoveDriveMountPoint();
  file_system_->Reload();
  drive_app_registry_->Update();

  // Reload() is asynchronous. But we can add back the mount point right away
  // because every operation waits until loading is complete.
  AddDriveMountPoint();
}

void DriveIntegrationService::AddDriveMountPoint() {
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
      file_system_proxy_.get(),
      drive_mount_point);

  if (success) {
    util::Log("Drive mount point is added");
    FOR_EACH_OBSERVER(DriveIntegrationServiceObserver, observers_,
                      OnFileSystemMounted());
  }
}

void DriveIntegrationService::RemoveDriveMountPoint() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  job_list()->CancelAllJobs();

  FOR_EACH_OBSERVER(DriveIntegrationServiceObserver, observers_,
                    OnFileSystemBeingUnmounted());

  fileapi::ExternalMountPoints* mount_points =
      BrowserContext::GetMountPoints(profile_);
  DCHECK(mount_points);

  mount_points->RevokeFileSystem(
      util::GetDriveMountPointPath().BaseName().AsUTF8Unsafe());
  if (file_system_proxy_.get()) {
    file_system_proxy_->DetachFromFileSystem();
    file_system_proxy_ = NULL;
  }
  util::Log("Drive mount point is removed");
}

void DriveIntegrationService::InitializeAfterMetadataInitialized(
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != FILE_ERROR_OK) {
    LOG(WARNING) << "Failed to initialize. Disabling Drive : "
                 << FileErrorToString(error);
    DisableDrive();
    return;
  }

  content::DownloadManager* download_manager =
      g_browser_process->download_status_updater() ?
      BrowserContext::GetDownloadManager(profile_) : NULL;
  download_handler_->Initialize(
      download_manager,
      cache_root_directory_.Append(util::kTemporaryFileDirectory));

  // Register for Google Drive invalidation notifications.
  DriveNotificationManager* drive_notification_manager =
      DriveNotificationManagerFactory::GetForProfile(profile_);
  if (drive_notification_manager) {
    drive_notification_manager->AddObserver(this);
    const bool registered =
        drive_notification_manager->push_notification_registered();
    const char* status = (registered ? "registered" : "not registered");
    util::Log("Push notification is %s", status);
  }

  drive_app_registry_->Update();
  AddDriveMountPoint();
}

void DriveIntegrationService::DisableDrive() {
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

//===================== DriveIntegrationServiceFactory =======================

// static
DriveIntegrationService* DriveIntegrationServiceFactory::GetForProfile(
    Profile* profile) {
  DriveIntegrationService* service = GetForProfileRegardlessOfStates(profile);
  if (service && !service->IsDriveEnabled())
    return NULL;

  return service;
}

// static
DriveIntegrationService*
DriveIntegrationServiceFactory::GetForProfileRegardlessOfStates(
    Profile* profile) {
  return static_cast<DriveIntegrationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
DriveIntegrationService* DriveIntegrationServiceFactory::FindForProfile(
    Profile* profile) {
  DriveIntegrationService* service = FindForProfileRegardlessOfStates(profile);
  if (service && !service->IsDriveEnabled())
    return NULL;

  return service;
}

// static
DriveIntegrationService*
DriveIntegrationServiceFactory::FindForProfileRegardlessOfStates(
    Profile* profile) {
  return static_cast<DriveIntegrationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
DriveIntegrationServiceFactory* DriveIntegrationServiceFactory::GetInstance() {
  return Singleton<DriveIntegrationServiceFactory>::get();
}

// static
void DriveIntegrationServiceFactory::SetFactoryForTest(
    const FactoryCallback& factory_for_test) {
  GetInstance()->factory_for_test_ = factory_for_test;
}

DriveIntegrationServiceFactory::DriveIntegrationServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "DriveIntegrationService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DriveNotificationManagerFactory::GetInstance());
  DependsOn(DownloadServiceFactory::GetInstance());
}

DriveIntegrationServiceFactory::~DriveIntegrationServiceFactory() {
}

BrowserContextKeyedService*
DriveIntegrationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  DriveIntegrationService* service = NULL;
  if (factory_for_test_.is_null()) {
    service = new DriveIntegrationService(
        profile, NULL, base::FilePath(), NULL);
  } else {
    service = factory_for_test_.Run(profile);
  }

  service->Initialize();
  return service;
}

}  // namespace drive
