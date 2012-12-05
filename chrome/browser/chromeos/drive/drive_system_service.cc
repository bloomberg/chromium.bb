// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_system_service.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/drive_api_service.h"
#include "chrome/browser/chromeos/drive/drive_download_observer.h"
#include "chrome/browser/chromeos/drive/drive_file_system.h"
#include "chrome/browser/chromeos/drive/drive_file_system_proxy.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_prefetcher.h"
#include "chrome/browser/chromeos/drive/drive_sync_client.h"
#include "chrome/browser/chromeos/drive/drive_webapps_registry.h"
#include "chrome/browser/chromeos/drive/event_logger.h"
#include "chrome/browser/chromeos/drive/file_write_helper.h"
#include "chrome/browser/chromeos/drive/stale_cache_files_remover.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/google_apis/drive_api_util.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "chrome/browser/google_apis/gdata_wapi_service.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google/cacheinvalidation/types.pb.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/user_agent/user_agent_util.h"

using content::BrowserContext;
using content::BrowserThread;

namespace drive {
namespace {

static const size_t kEventLogHistorySize = 100;

// Used in test to setup system service.
google_apis::DriveServiceInterface* g_test_drive_service = NULL;
const std::string* g_test_cache_root = NULL;

// The sync invalidation object ID for Google Drive.
const char kDriveInvalidationObjectId[] = "CHANGELOG";

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

  return base::StringPrintf("%s-%s %s (%s)",
                            kDriveClientName,
                            version.c_str(),
                            kLibraryInfo,
                            os_cpu_info.c_str());
}

}  // namespace

DriveSystemService::DriveSystemService(Profile* profile)
    : profile_(profile),
      drive_disabled_(false),
      push_notification_registered_(false),
      cache_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::SequencedWorkerPool* blocking_pool = BrowserThread::GetBlockingPool();
  blocking_task_runner_ = blocking_pool->GetSequencedTaskRunner(
      blocking_pool->GetSequenceToken());
}

DriveSystemService::~DriveSystemService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cache_->Destroy();
}

void DriveSystemService::Initialize(
    google_apis::DriveServiceInterface* drive_service,
    const FilePath& cache_root) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  event_logger_.reset(new EventLogger(kEventLogHistorySize));
  drive_service_.reset(drive_service);
  cache_ = DriveCache::CreateDriveCache(cache_root, blocking_task_runner_);
  uploader_.reset(new google_apis::DriveUploader(drive_service_.get()));
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
  prefetcher_.reset(new DrivePrefetcher(file_system(),
                                        event_logger(),
                                        DrivePrefetcherOptions()));
  sync_client_->AddObserver(prefetcher_.get());
  stale_cache_files_remover_.reset(new StaleCacheFilesRemover(file_system(),
                                                              cache()));

  sync_client_->Initialize();
  file_system_->Initialize();
  cache_->RequestInitialize(base::Bind(&DriveSystemService::OnCacheInitialized,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void DriveSystemService::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ProfileSyncService* profile_sync_service =
      profile_ ? ProfileSyncServiceFactory::GetForProfile(profile_) : NULL;
  if (profile_sync_service && push_notification_registered_) {
    // TODO(kochi): Once DriveSystemService gets started / stopped at runtime,
    // this ID needs to be unregistered *before* the handler is unregistered
    // as ID persists across browser restarts.
    if (!IsDriveEnabledForProfile(profile_)) {
      profile_sync_service->UpdateRegisteredInvalidationIds(
          this, syncer::ObjectIdSet());
    }
    profile_sync_service->UnregisterInvalidationHandler(this);
    push_notification_registered_ = false;
  }

  RemoveDriveMountPoint();

  // Shut down the member objects in the reverse order of creation.
  stale_cache_files_remover_.reset();
  sync_client_->RemoveObserver(prefetcher_.get());
  prefetcher_.reset();
  sync_client_.reset();
  download_observer_.reset();
  file_write_helper_.reset();
  file_system_.reset();
  webapps_registry_.reset();
  uploader_.reset();
  drive_service_.reset();
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

void DriveSystemService::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  file_system_->SetPushNotificationEnabled(
      state == syncer::INVALIDATIONS_ENABLED);
}

void DriveSystemService::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map,
    syncer::IncomingInvalidationSource source) {
  DCHECK_EQ(1U, invalidation_map.size());
  const invalidation::ObjectId object_id(
      ipc::invalidation::ObjectSource::COSMO_CHANGELOG,
      kDriveInvalidationObjectId);
  DCHECK_EQ(1U, invalidation_map.count(object_id));

  file_system_->CheckForUpdates();
}

void DriveSystemService::ClearCacheAndRemountFileSystem(
    const base::Callback<void(bool)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RemoveDriveMountPoint();
  drive_service()->CancelAll();
  cache_->ClearAll(base::Bind(&DriveSystemService::AddBackDriveMountPoint,
                              weak_ptr_factory_.GetWeakPtr(),
                              callback));
}

void DriveSystemService::AddBackDriveMountPoint(
    const base::Callback<void(bool)>& callback,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_system_->Initialize();
  AddDriveMountPoint();

  if (!callback.is_null())
    callback.Run(success);
}

void DriveSystemService::ReloadAndRemountFileSystem() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RemoveDriveMountPoint();
  drive_service()->CancelAll();
  file_system_->Reload();

  // Reload() is asynchronous. But we can add back the mount point right away
  // because every operation waits until loading is complete.
  AddDriveMountPoint();
}

void DriveSystemService::AddDriveMountPoint() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const FilePath mount_point = util::GetDriveMountPointPath();
  fileapi::ExternalFileSystemMountPointProvider* provider =
      BrowserContext::GetDefaultStoragePartition(profile_)->
          GetFileSystemContext()->external_provider();
  if (provider && !provider->HasMountPoint(mount_point)) {
    event_logger_->Log("AddDriveMountPoint");
    provider->AddRemoteMountPoint(
        mount_point,
        new DriveFileSystemProxy(file_system_.get()));
  }

  file_system_->NotifyFileSystemMounted();
}

void DriveSystemService::RemoveDriveMountPoint() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_system_->NotifyFileSystemToBeUnmounted();
  file_system_->StopPolling();

  const FilePath mount_point = util::GetDriveMountPointPath();
  fileapi::ExternalFileSystemMountPointProvider* provider =
      BrowserContext::GetDefaultStoragePartition(profile_)->
          GetFileSystemContext()->external_provider();
  if (provider && provider->HasMountPoint(mount_point)) {
    provider->RemoveMountPoint(mount_point);
    event_logger_->Log("RemoveDriveMountPoint");
  }
}

void DriveSystemService::OnCacheInitialized(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!success) {
    LOG(WARNING) << "Failed to initialize the cache. Disabling Drive";
    DisableDrive();
    return;
  }

  content::DownloadManager* download_manager =
    g_browser_process->download_status_updater() ?
        BrowserContext::GetDownloadManager(profile_) : NULL;
  download_observer_->Initialize(
      download_manager,
      cache_->GetCacheDirectoryPath(
          DriveCache::CACHE_TYPE_TMP_DOWNLOADS));

  // Register for Google Drive invalidation notifications.
  ProfileSyncService* profile_sync_service =
      profile_ ? ProfileSyncServiceFactory::GetForProfile(profile_) : NULL;
  if (profile_sync_service) {
    DCHECK(!push_notification_registered_);
    profile_sync_service->RegisterInvalidationHandler(this);
    syncer::ObjectIdSet ids;
    ids.insert(invalidation::ObjectId(
        ipc::invalidation::ObjectSource::COSMO_CHANGELOG,
        kDriveInvalidationObjectId));
    profile_sync_service->UpdateRegisteredInvalidationIds(this, ids);
    push_notification_registered_ = true;
    file_system_->SetPushNotificationEnabled(
        profile_sync_service->GetInvalidatorState() ==
        syncer::INVALIDATIONS_ENABLED);
  }

  AddDriveMountPoint();

  // Start prefetching of Drive metadata.
  file_system_->StartInitialFeedFetch();
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
  DriveSystemService* service = static_cast<DriveSystemService*>(
      GetInstance()->GetServiceForProfile(profile, true));
  if (service && !service->IsDriveEnabled())
    return NULL;

  return service;
}

// static
DriveSystemService* DriveSystemServiceFactory::FindForProfile(
    Profile* profile) {
  DriveSystemService* service = static_cast<DriveSystemService*>(
      GetInstance()->GetServiceForProfile(profile, false));
  if (service && !service->IsDriveEnabled())
    return NULL;

  return service;
}

// static
DriveSystemServiceFactory* DriveSystemServiceFactory::GetInstance() {
  return Singleton<DriveSystemServiceFactory>::get();
}

DriveSystemServiceFactory::DriveSystemServiceFactory()
    : ProfileKeyedServiceFactory("DriveSystemService",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(DownloadServiceFactory::GetInstance());
}

DriveSystemServiceFactory::~DriveSystemServiceFactory() {
}

// static
void DriveSystemServiceFactory::set_drive_service_for_test(
    google_apis::DriveServiceInterface* drive_service) {
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

  google_apis::DriveServiceInterface* drive_service = g_test_drive_service;
  g_test_drive_service = NULL;
  if (!drive_service) {
    if (google_apis::util::IsDriveV2ApiEnabled()) {
      drive_service = new DriveAPIService(
          g_browser_process->system_request_context(),
          GetDriveUserAgent());
    } else {
      drive_service = new google_apis::GDataWapiService(
          g_browser_process->system_request_context(),
          GURL(google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction),
          GetDriveUserAgent());
    }
  }

  FilePath cache_root =
      g_test_cache_root ? FilePath(*g_test_cache_root) :
                          DriveCache::GetCacheRootPath(profile);
  delete g_test_cache_root;
  g_test_cache_root = NULL;

  service->Initialize(drive_service, cache_root);
  return service;
}

}  // namespace drive
