// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_integration_service.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/md5.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/drive/debug_info_collector.h"
#include "chrome/browser/chromeos/drive/download_handler.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/profiles/profile_util.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/drivefs/drivefs_bootstrap.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/drive/chromeos/file_cache.h"
#include "components/drive/chromeos/file_system.h"
#include "components/drive/chromeos/file_system_observer.h"
#include "components/drive/chromeos/resource_metadata.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/drive_notification_manager.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/event_logger.h"
#include "components/drive/job_scheduler.h"
#include "components/drive/resource_metadata_storage.h"
#include "components/drive/service/drive_api_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/user_agent.h"
#include "google_apis/drive/auth_service.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserContext;
using content::BrowserThread;

namespace drive {
namespace {

// Name of the directory used to store metadata.
const base::FilePath::CharType kMetadataDirectory[] = FILE_PATH_LITERAL("meta");

// Name of the directory used to store cached files.
const base::FilePath::CharType kCacheFileDirectory[] =
    FILE_PATH_LITERAL("files");

// Name of the directory used to store temporary files.
const base::FilePath::CharType kTemporaryFileDirectory[] =
    FILE_PATH_LITERAL("tmp");

// Returns a user agent string used for communicating with the Drive backend,
// both WAPI and Drive API.  The user agent looks like:
//
// chromedrive-<VERSION> chrome-cc/none (<OS_CPU_INFO>)
// chromedrive-24.0.1274.0 chrome-cc/none (CrOS x86_64 0.4.0)
//
// TODO(satorux): Move this function to somewhere else: crbug.com/151605
std::string GetDriveUserAgent() {
  const char kDriveClientName[] = "chromedrive";

  const std::string version = version_info::GetVersionNumber();

  // This part is <client_name>/<version>.
  const char kLibraryInfo[] = "chrome-cc/none";

  const std::string os_cpu_info =
      content::BuildOSCpuInfo(false /* include_android_build_number */);

  // Add "gzip" to receive compressed data from the server.
  // (see https://developers.google.com/drive/performance)
  return base::StringPrintf("%s-%s %s (%s) (gzip)",
                            kDriveClientName,
                            version.c_str(),
                            kLibraryInfo,
                            os_cpu_info.c_str());
}

void DeleteDirectoryContents(const base::FilePath& dir) {
  base::FileEnumerator content_enumerator(
      dir, false, base::FileEnumerator::FILES |
                      base::FileEnumerator::DIRECTORIES |
                      base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath path = content_enumerator.Next(); !path.empty();
       path = content_enumerator.Next()) {
    base::DeleteFile(path, true);
  }
}

base::FilePath FindUniquePath(const base::FilePath& base_name) {
  auto target = base_name;
  for (int uniquifier = 1; base::PathExists(target); ++uniquifier) {
    target = base_name.InsertBeforeExtensionASCII(
        base::StringPrintf(" (%d)", uniquifier));
  }
  return target;
}

base::FilePath GetRecoveredFilesPath(
    const base::FilePath& downloads_directory) {
  const std::string& dest_directory_name = l10n_util::GetStringUTF8(
      IDS_FILE_BROWSER_RECOVERED_FILES_FROM_GOOGLE_DRIVE_DIRECTORY_NAME);
  return FindUniquePath(downloads_directory.Append(dest_directory_name));
}

// Initializes FileCache and ResourceMetadata.
// Must be run on the same task runner used by |cache| and |resource_metadata|.
FileError InitializeMetadata(
    const base::FilePath& cache_root_directory,
    internal::ResourceMetadataStorage* metadata_storage,
    internal::FileCache* cache,
    internal::ResourceMetadata* resource_metadata,
    const base::FilePath& downloads_directory) {
  if (!base::DirectoryExists(
          cache_root_directory.Append(kTemporaryFileDirectory))) {
    if (base::SysInfo::IsRunningOnChromeOS())
      LOG(ERROR) << "/tmp should have been created as clear.";
    // Create /tmp directory as encrypted. Cryptohome will re-create /tmp
    // direcotry at the next login.
    if (!base::CreateDirectory(
            cache_root_directory.Append(kTemporaryFileDirectory))) {
      LOG(WARNING) << "Failed to create directories.";
      return FILE_ERROR_FAILED;
    }
  }
  // Files in temporary directory need not persist across sessions. Clean up
  // the directory content while initialization. The directory itself should not
  // be deleted because it's created by cryptohome in clear and shouldn't be
  // re-created as encrypted.
  DeleteDirectoryContents(cache_root_directory.Append(kTemporaryFileDirectory));
  if (!base::CreateDirectory(cache_root_directory.Append(
          kMetadataDirectory)) ||
      !base::CreateDirectory(cache_root_directory.Append(
          kCacheFileDirectory))) {
    LOG(WARNING) << "Failed to create directories.";
    return FILE_ERROR_FAILED;
  }

  // Change permissions of cache file directory to u+rwx,og+x (711) in order to
  // allow archive files in that directory to be mounted by cros-disks.
  base::SetPosixFilePermissions(
      cache_root_directory.Append(kCacheFileDirectory),
      base::FILE_PERMISSION_USER_MASK |
      base::FILE_PERMISSION_EXECUTE_BY_GROUP |
      base::FILE_PERMISSION_EXECUTE_BY_OTHERS);

  // If attempting to migrate to DriveFS without previous Drive sync data
  // present, skip the migration.
  if (base::IsDirectoryEmpty(cache_root_directory.Append(kMetadataDirectory)) &&
      !cache) {
    return FILE_ERROR_FAILED;
  }

  internal::ResourceMetadataStorage::UpgradeOldDB(
      metadata_storage->directory_path());

  if (!metadata_storage->Initialize()) {
    LOG(WARNING) << "Failed to initialize the metadata storage.";
    return FILE_ERROR_FAILED;
  }

  // When running with DriveFS and migrating pinned files, only
  // |metadata_storage| is non-null and needs to be initialized.
  if (!cache)
    return FILE_ERROR_OK;

  if (!cache->Initialize()) {
    LOG(WARNING) << "Failed to initialize the cache.";
    return FILE_ERROR_FAILED;
  }

  if (metadata_storage->cache_file_scan_is_needed()) {
    // Generate unique directory name.
    auto dest_directory = GetRecoveredFilesPath(downloads_directory);

    internal::ResourceMetadataStorage::RecoveredCacheInfoMap
        recovered_cache_info;
    metadata_storage->RecoverCacheInfoFromTrashedResourceMap(
        &recovered_cache_info);

    LOG_IF(WARNING, !recovered_cache_info.empty())
        << "DB could not be opened for some reasons. "
        << "Recovering cache files to " << dest_directory.value();
    if (!cache->RecoverFilesFromCacheDirectory(dest_directory,
                                               recovered_cache_info)) {
      LOG(WARNING) << "Failed to recover cache files.";
      return FILE_ERROR_FAILED;
    }
  }

  FileError error = resource_metadata->Initialize();
  LOG_IF(WARNING, error != FILE_ERROR_OK)
      << "Failed to initialize resource metadata. " << FileErrorToString(error);
  return error;
}

void ResetCacheDone(base::OnceCallback<void(bool)> callback, FileError error) {
  std::move(callback).Run(error == FILE_ERROR_OK);
}

base::FilePath GetFullPath(internal::ResourceMetadataStorage* metadata_storage,
                           const ResourceEntry& entry) {
  std::vector<std::string> path_components;
  ResourceEntry current_entry = entry;
  constexpr int kPathComponentSanityLimit = 100;
  for (int i = 0; i < kPathComponentSanityLimit; ++i) {
    path_components.push_back(current_entry.base_name());
    if (!current_entry.has_parent_local_id()) {
      // Ignore anything not contained within the drive grand root.
      return {};
    }
    if (current_entry.parent_local_id() == util::kDriveGrandRootLocalId) {
      // Omit the DriveGrantRoot directory from the path; DriveFS paths are
      // relative to the mount point.
      break;
    }
    if (metadata_storage->GetEntry(current_entry.parent_local_id(),
                                   &current_entry) != FILE_ERROR_OK) {
      return {};
    }
  }
  if (path_components.empty()) {
    return {};
  }
  base::FilePath path("/");
  for (auto it = path_components.crbegin(); it != path_components.crend();
       ++it) {
    path = path.Append(*it);
  }
  return path;
}

// Recover any dirty files in GCache/v1 to a recovered files directory in
// Downloads. This imitates the behavior of recovering cache files when database
// corruption occurs; however, in this case, we have an intact database so can
// use the exact file names, potentially with uniquifiers added since the
// directory structure is discarded.
void RecoverDirtyFiles(
    const base::FilePath& cache_directory,
    const base::FilePath& downloads_directory,
    const std::vector<std::pair<base::FilePath, std::string>>& dirty_files) {
  if (dirty_files.empty()) {
    return;
  }
  auto recovery_directory = GetRecoveredFilesPath(downloads_directory);
  if (!base::CreateDirectory(recovery_directory)) {
    return;
  }
  for (auto& dirty_file : dirty_files) {
    auto target_path =
        FindUniquePath(recovery_directory.Append(dirty_file.first.BaseName()));
    base::Move(cache_directory.Append(dirty_file.second), target_path);
  }
}

// Remove the data used by the old Drive client, first moving any dirty files
// into the user's Downloads.
void CleanupGCacheV1(
    const base::FilePath& cache_directory,
    const base::FilePath& downloads_directory,
    std::vector<std::pair<base::FilePath, std::string>> dirty_files) {
  RecoverDirtyFiles(cache_directory.Append(kCacheFileDirectory),
                    downloads_directory, dirty_files);
  DeleteDirectoryContents(cache_directory);
}

std::vector<base::FilePath> GetPinnedFiles(
    std::unique_ptr<internal::ResourceMetadataStorage, util::DestroyHelper>
        metadata_storage,
    base::FilePath cache_directory,
    base::FilePath downloads_directory) {
  std::vector<base::FilePath> pinned_files;
  std::vector<std::pair<base::FilePath, std::string>> dirty_files;
  for (auto it = metadata_storage->GetIterator(); !it->IsAtEnd();
       it->Advance()) {
    const auto& value = it->GetValue();
    if (!value.has_file_specific_info()) {
      continue;
    }
    const auto& info = value.file_specific_info();
    if (info.cache_state().is_pinned()) {
      auto path = GetFullPath(metadata_storage.get(), value);
      if (!path.empty()) {
        pinned_files.push_back(std::move(path));
      }
    }
    if (info.cache_state().is_dirty()) {
      dirty_files.push_back(std::make_pair(
          GetFullPath(metadata_storage.get(), value), value.local_id()));
    }
  }
  // Destructing |metadata_storage| requires a posted task to run, so defer
  // deleting its data until after it's been destructed. This also returns the
  // list of files to pin to the UI thread without waiting for the remaining
  // data to be cleared.
  metadata_storage.reset();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&CleanupGCacheV1, std::move(cache_directory),
                     std::move(downloads_directory), std::move(dirty_files)));
  return pinned_files;
}

DriveMountStatus ConvertMountFailure(
    drivefs::DriveFsHost::MountObserver::MountFailure failure) {
  switch (failure) {
    case drivefs::DriveFsHost::MountObserver::MountFailure::kInvocation:
      return DriveMountStatus::kInvocationFailure;
    case drivefs::DriveFsHost::MountObserver::MountFailure::kIpcDisconnect:
      return DriveMountStatus::kUnexpectedDisconnect;
    case drivefs::DriveFsHost::MountObserver::MountFailure::kNeedsRestart:
      return DriveMountStatus::kTemporaryUnavailable;
    case drivefs::DriveFsHost::MountObserver::MountFailure::kTimeout:
      return DriveMountStatus::kTimeout;
    case drivefs::DriveFsHost::MountObserver::MountFailure::kUnknown:
      return DriveMountStatus::kUnknownFailure;
  }
  NOTREACHED();
}

void UmaEmitMountStatus(DriveMountStatus status) {
  UMA_HISTOGRAM_ENUMERATION("DriveCommon.Lifecycle.Mount", status);
}

void UmaEmitMountTime(DriveMountStatus status,
                      const base::TimeTicks& time_started) {
  if (status == DriveMountStatus::kSuccess) {
    UMA_HISTOGRAM_MEDIUM_TIMES("DriveCommon.Lifecycle.MountTime.SuccessTime",
                               base::TimeTicks::Now() - time_started);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES("DriveCommon.Lifecycle.MountTime.FailTime",
                               base::TimeTicks::Now() - time_started);
  }
}

void UmaEmitMountOutcome(DriveMountStatus status,
                         const base::TimeTicks& time_started) {
  UmaEmitMountStatus(status);
  UmaEmitMountTime(status, time_started);
}

void UmaEmitUnmountOutcome(DriveMountStatus status) {
  UMA_HISTOGRAM_ENUMERATION("DriveCommon.Lifecycle.Unmount", status);
}

void UmaEmitFirstLaunch(const base::TimeTicks& time_started) {
  UMA_HISTOGRAM_MEDIUM_TIMES("DriveCommon.Lifecycle.FirstLaunchTime",
                             base::TimeTicks::Now() - time_started);
}

}  // namespace

// Observes drive disable Preference's change.
class DriveIntegrationService::PreferenceWatcher
    : public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public chromeos::NetworkPortalDetector::Observer {
 public:
  explicit PreferenceWatcher(PrefService* pref_service)
      : pref_service_(pref_service),
        integration_service_(nullptr),
        weak_ptr_factory_(this) {
    DCHECK(pref_service);
    pref_change_registrar_.Init(pref_service);
    pref_change_registrar_.Add(
        prefs::kDisableDrive,
        base::BindRepeating(&PreferenceWatcher::OnPreferenceChanged,
                            weak_ptr_factory_.GetWeakPtr()));
    pref_change_registrar_.Add(
        prefs::kDisableDriveOverCellular,
        base::BindRepeating(&PreferenceWatcher::UpdateSyncPauseState,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  ~PreferenceWatcher() override {
    if (integration_service_ && integration_service_->drivefs_holder_) {
      content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(
          this);
      chromeos::network_portal_detector::GetInstance()->RemoveObserver(this);
    }
  }

  void set_integration_service(DriveIntegrationService* integration_service) {
    integration_service_ = integration_service;
    if (integration_service_->drivefs_holder_) {
      content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(
          this);

      // The NetworkPortalDetector instance may not be ready yet, so defer
      // accessing it.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&DriveIntegrationService::PreferenceWatcher::
                             AddNetworkPortalDetectorObserver,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  void UpdateSyncPauseState() {
    auto type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
    if (content::GetNetworkConnectionTracker()->GetConnectionType(
            &type, base::BindOnce(&DriveIntegrationService::PreferenceWatcher::
                                      OnConnectionChanged,
                                  weak_ptr_factory_.GetWeakPtr()))) {
      OnConnectionChanged(type);
    }
  }

  bool is_offline() const {
    return last_portal_status_ !=
               chromeos::NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE &&
           last_portal_status_ !=
               chromeos::NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN;
  }

 private:
  void OnPreferenceChanged() {
    DCHECK(integration_service_);
    integration_service_->SetEnabled(
        !pref_service_->GetBoolean(prefs::kDisableDrive));
  }

  void AddNetworkPortalDetectorObserver() {
    chromeos::network_portal_detector::GetInstance()->AddAndFireObserver(this);
  }

  // chromeos::NetworkPortalDetector::Observer
  void OnPortalDetectionCompleted(
      const chromeos::NetworkState* network,
      const chromeos::NetworkPortalDetector::CaptivePortalState& state)
      override {
    last_portal_status_ = state.status;

    if (integration_service_->remount_when_online_ &&
        last_portal_status_ ==
            chromeos::NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE) {
      integration_service_->remount_when_online_ = false;
      integration_service_->mount_start_ = {};
      integration_service_->AddDriveMountPoint();
    }
  }

  // network::NetworkConnectionTracker::NetworkConnectionObserver
  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    if (!integration_service_->GetDriveFsInterface())
      return;

    integration_service_->GetDriveFsInterface()->UpdateNetworkState(
        network::NetworkConnectionTracker::IsConnectionCellular(type) &&
            pref_service_->GetBoolean(prefs::kDisableDriveOverCellular),
        type == network::mojom::ConnectionType::CONNECTION_NONE);
  }

  PrefService* pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  DriveIntegrationService* integration_service_;
  chromeos::NetworkPortalDetector::CaptivePortalStatus last_portal_status_ =
      chromeos::NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN;

  base::WeakPtrFactory<PreferenceWatcher> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(PreferenceWatcher);
};

class DriveIntegrationService::DriveFsHolder
    : public drivefs::DriveFsHost::Delegate,
      public drivefs::DriveFsHost::MountObserver {
 public:
  DriveFsHolder(Profile* profile,
                drivefs::DriveFsHost::MountObserver* mount_observer,
                DriveFsMojoListenerFactory test_drivefs_mojo_listener_factory)
      : profile_(profile),
        mount_observer_(mount_observer),
        test_drivefs_mojo_listener_factory_(
            std::move(test_drivefs_mojo_listener_factory)),
        drivefs_host_(profile_->GetPath(),
                      this,
                      this,
                      base::DefaultClock::GetInstance(),
                      chromeos::disks::DiskMountManager::GetInstance(),
                      std::make_unique<base::OneShotTimer>()) {}

  drivefs::DriveFsHost* drivefs_host() { return &drivefs_host_; }

 private:
  // drivefs::DriveFsHost::Delegate:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return profile_->GetURLLoaderFactory();
  }

  service_manager::Connector* GetConnector() override {
    return content::BrowserContext::GetConnectorFor(profile_);
  }

  const AccountId& GetAccountId() override {
    return chromeos::ProfileHelper::Get()
        ->GetUserByProfile(profile_)
        ->GetAccountId();
  }

  std::string GetObfuscatedAccountId() override {
    return base::MD5String(GetProfileSalt() + "-" +
                           GetAccountId().GetAccountIdKey());
  }

  DriveNotificationManager& GetDriveNotificationManager() override {
    return *DriveNotificationManagerFactory::GetForBrowserContext(profile_);
  }

  void OnMountFailed(MountFailure failure,
                     base::Optional<base::TimeDelta> remount_delay) override {
    mount_observer_->OnMountFailed(failure, std::move(remount_delay));
  }

  void OnMounted(const base::FilePath& path) override {
    mount_observer_->OnMounted(path);
  }

  void OnUnmounted(base::Optional<base::TimeDelta> remount_delay) override {
    mount_observer_->OnUnmounted(std::move(remount_delay));
  }

  const std::string& GetProfileSalt() {
    if (!profile_salt_.empty()) {
      return profile_salt_;
    }
    PrefService* prefs = profile_->GetPrefs();
    profile_salt_ = prefs->GetString(prefs::kDriveFsProfileSalt);
    if (profile_salt_.empty()) {
      profile_salt_ = base::UnguessableToken::Create().ToString();
      prefs->SetString(prefs::kDriveFsProfileSalt, profile_salt_);
    }
    return profile_salt_;
  }

  std::unique_ptr<drivefs::DriveFsBootstrapListener> CreateMojoListener()
      override {
    if (test_drivefs_mojo_listener_factory_)
      return test_drivefs_mojo_listener_factory_.Run();
    return Delegate::CreateMojoListener();
  }

  Profile* const profile_;
  drivefs::DriveFsHost::MountObserver* const mount_observer_;

  const DriveFsMojoListenerFactory test_drivefs_mojo_listener_factory_;

  drivefs::DriveFsHost drivefs_host_;

  std::string profile_salt_;

  DISALLOW_COPY_AND_ASSIGN(DriveFsHolder);
};

class DriveIntegrationService::NotificationManager : public FileSystemObserver {
 public:
  // TODO(slangley): Allow for testing of DriveNotificationManager by using
  // dependency injection.
  explicit NotificationManager(Profile* profile) : profile_(profile) {}
  ~NotificationManager() override = default;

  void OnTeamDrivesUpdated(
      const std::set<std::string>& added_team_drive_ids,
      const std::set<std::string>& removed_team_drive_ids) override {
    DriveNotificationManager* drive_notification_manager =
        DriveNotificationManagerFactory::FindForBrowserContext(profile_);

    if (drive_notification_manager) {
      drive_notification_manager->UpdateTeamDriveIds(added_team_drive_ids,
                                                     removed_team_drive_ids);
    }
  }

 private:
  Profile* const profile_;  // Not Owned

  DISALLOW_COPY_AND_ASSIGN(NotificationManager);
};

DriveIntegrationService::DriveIntegrationService(
    Profile* profile,
    PreferenceWatcher* preference_watcher,
    DriveServiceInterface* test_drive_service,
    const std::string& test_mount_point_name,
    const base::FilePath& test_cache_root,
    FileSystemInterface* test_file_system,
    DriveFsMojoListenerFactory test_drivefs_mojo_listener_factory)
    : profile_(profile),
      state_(NOT_INITIALIZED),
      enabled_(false),
      mount_point_name_(test_mount_point_name),
      cache_root_directory_(!test_cache_root.empty()
                                ? test_cache_root
                                : util::GetCacheRootPath(profile)),
      drivefs_holder_(base::FeatureList::IsEnabled(chromeos::features::kDriveFs)
                          ? std::make_unique<DriveFsHolder>(
                                profile_,
                                this,
                                std::move(test_drivefs_mojo_listener_factory))
                          : nullptr),
      preference_watcher_(preference_watcher),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile && !profile->IsOffTheRecord());

  logger_ = std::make_unique<EventLogger>();
  blocking_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::WithBaseSyncPrimitives()});

  if (preference_watcher_)
    preference_watcher->set_integration_service(this);

  bool migrated_to_drivefs =
      profile_->GetPrefs()->GetBoolean(prefs::kDriveFsPinnedMigrated);
  if (!drivefs_holder_ || !migrated_to_drivefs) {
    metadata_storage_.reset(new internal::ResourceMetadataStorage(
        cache_root_directory_.Append(kMetadataDirectory),
        blocking_task_runner_.get()));
  }
  if (drivefs_holder_) {
    delete test_drive_service;
    delete test_file_system;
    if (migrated_to_drivefs) {
      state_ = INITIALIZED;
    }
    SetEnabled(drive::util::IsDriveEnabledForProfile(profile));
    return;
  }

  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (test_drive_service) {
    drive_service_.reset(test_drive_service);
  } else {
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
    if (g_browser_process->system_network_context_manager()) {
      // system_network_context_manager() returns nullptr in unit-tests.
      url_loader_factory = g_browser_process->system_network_context_manager()
                               ->GetSharedURLLoaderFactory();
    }
    drive_service_ = std::make_unique<DriveAPIService>(
        identity_manager, url_loader_factory, blocking_task_runner_.get(),
        GURL(google_apis::DriveApiUrlGenerator::kBaseUrlForProduction),
        GURL(google_apis::DriveApiUrlGenerator::kBaseThumbnailUrlForProduction),
        GetDriveUserAgent(), NO_TRAFFIC_ANNOTATION_YET);
  }

  device::mojom::WakeLockProviderPtr wake_lock_provider;
  DCHECK(content::ServiceManagerConnection::GetForProcess());
  auto* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(device::mojom::kServiceName,
                           mojo::MakeRequest(&wake_lock_provider));

  scheduler_ = std::make_unique<JobScheduler>(
      profile_->GetPrefs(), logger_.get(), drive_service_.get(),
      content::GetNetworkConnectionTracker(), blocking_task_runner_.get(),
      std::move(wake_lock_provider));
  cache_.reset(new internal::FileCache(
      metadata_storage_.get(),
      cache_root_directory_.Append(kCacheFileDirectory),
      blocking_task_runner_.get(), nullptr /* free_disk_space_getter */));

  resource_metadata_.reset(new internal::ResourceMetadata(
      metadata_storage_.get(), cache_.get(), blocking_task_runner_));

  file_system_.reset(
      test_file_system
          ? test_file_system
          : new FileSystem(
                logger_.get(), cache_.get(), scheduler_.get(),
                resource_metadata_.get(), blocking_task_runner_.get(),
                cache_root_directory_.Append(kTemporaryFileDirectory)));
  download_handler_ = std::make_unique<DownloadHandler>(file_system());
  debug_info_collector_ = std::make_unique<DebugInfoCollector>(
      resource_metadata_.get(), file_system(), blocking_task_runner_.get());

  notification_manager_ =
      std::make_unique<DriveIntegrationService::NotificationManager>(profile);

  file_system_->AddObserver(notification_manager_.get());

  SetEnabled(drive::util::IsDriveEnabledForProfile(profile));
}

DriveIntegrationService::~DriveIntegrationService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void DriveIntegrationService::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  weak_ptr_factory_.InvalidateWeakPtrs();

  DriveNotificationManager* drive_notification_manager =
      DriveNotificationManagerFactory::FindForBrowserContext(profile_);
  if (drive_notification_manager)
    drive_notification_manager->RemoveObserver(this);

  RemoveDriveMountPoint();
  notification_manager_.reset();
  debug_info_collector_.reset();
  download_handler_.reset();
  file_system_.reset();
  scheduler_.reset();
  drive_service_.reset();
}

void DriveIntegrationService::SetEnabled(bool enabled) {
  // If Drive is being disabled, ensure the download destination preference to
  // be out of Drive. Do this before "Do nothing if not changed." because we
  // want to run the check for the first SetEnabled() called in the constructor,
  // which may be a change from false to false.
  if (!enabled)
    AvoidDriveAsDownloadDirectoryPreference();

  // Do nothing if not changed.
  if (enabled_ == enabled)
    return;

  if (enabled) {
    enabled_ = true;
    switch (state_) {
      case NOT_INITIALIZED:
        // If the initialization is not yet done, trigger it.
        Initialize();
        return;

      case INITIALIZING:
      case REMOUNTING:
        // If the state is INITIALIZING or REMOUNTING, at the end of the
        // process, it tries to mounting (with re-checking enabled state).
        // Do nothing for now.
        return;

      case INITIALIZED:
        // The integration service is already initialized. Add the mount point.
        AddDriveMountPoint();
        return;
    }
    NOTREACHED();
  } else {
    RemoveDriveMountPoint();
    enabled_ = false;
    drivefs_total_failures_count_ = 0;
    drivefs_consecutive_failures_count_ = 0;
    mount_failed_ = false;
    mount_start_ = {};
  }
}

bool DriveIntegrationService::IsMounted() const {
  if (mount_point_name_.empty())
    return false;

  // Look up the registered path, and just discard it.
  // GetRegisteredPath() returns true if the path is available.
  base::FilePath unused;
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);
  return mount_points->GetRegisteredPath(mount_point_name_, &unused);
}

base::FilePath DriveIntegrationService::GetMountPointPath() const {
  return drivefs_holder_ ? drivefs_holder_->drivefs_host()->GetMountPath()
                         : drive::util::GetDriveMountPointPath(profile_);
}

base::FilePath DriveIntegrationService::GetDriveFsLogPath() const {
  return drivefs_holder_
             ? drivefs_holder_->drivefs_host()->GetDataPath().Append(
                   "Logs/drivefs.txt")
             : base::FilePath();
}

bool DriveIntegrationService::GetRelativeDrivePath(
    const base::FilePath& local_path,
    base::FilePath* drive_path) const {
  if (!IsMounted()) {
    return false;
  }
  base::FilePath mount_point = GetMountPointPath();
  base::FilePath relative("/");
  if (!mount_point.AppendRelativePath(local_path, &relative)) {
    return false;
  }
  if (drive_path) {
    *drive_path = relative;
  }
  return true;
}

void DriveIntegrationService::AddObserver(
    DriveIntegrationServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

void DriveIntegrationService::RemoveObserver(
    DriveIntegrationServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

void DriveIntegrationService::OnNotificationReceived(
    const std::map<std::string, int64_t>& invalidations) {
  logger_->Log(logging::LOG_INFO,
               "Received Drive update notification. Will check for update.");
  std::set<std::string> ids;
  for (auto& invalidation : invalidations) {
    ids.insert(invalidation.first);
  }
  file_system_->CheckForUpdates(ids);
}

void DriveIntegrationService::OnNotificationTimerFired() {
  logger_->Log(logging::LOG_INFO,
               "Drive notification timer erpired. Will check all for update.");
  file_system_->CheckForUpdates();
}

void DriveIntegrationService::OnPushNotificationEnabled(bool enabled) {
  const char* status = (enabled ? "enabled" : "disabled");
  logger_->Log(logging::LOG_INFO, "Push notification is %s", status);
}

void DriveIntegrationService::ClearCacheAndRemountFileSystem(
    const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(callback);

  if (state_ != INITIALIZED) {
    callback.Run(false);
    return;
  } else if (drivefs_holder_) {
    GetDriveFsInterface()->ResetCache(
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(&ResetCacheDone, callback), FILE_ERROR_ABORT));
    return;
  }

  RemoveDriveMountPoint();

  state_ = REMOUNTING;
  // Resetting the file system clears resource metadata and cache.
  file_system_->Reset(base::Bind(
      &DriveIntegrationService::AddBackDriveMountPoint,
      weak_ptr_factory_.GetWeakPtr(),
      callback));
}

drivefs::DriveFsHost* DriveIntegrationService::GetDriveFsHost() const {
  if (!drivefs_holder_)
    return nullptr;
  return drivefs_holder_->drivefs_host();
}

drivefs::mojom::DriveFs* DriveIntegrationService::GetDriveFsInterface() const {
  if (!drivefs_holder_)
    return nullptr;
  return drivefs_holder_->drivefs_host()->GetDriveFsInterface();
}

void DriveIntegrationService::AddBackDriveMountPoint(
    const base::Callback<void(bool)>& callback,
    FileError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(callback);

  state_ = error == FILE_ERROR_OK ? INITIALIZED : NOT_INITIALIZED;

  if (error != FILE_ERROR_OK || !enabled_) {
    // Failed to reset, or Drive was disabled during the reset.
    callback.Run(false);
    return;
  }

  AddDriveMountPoint();
  callback.Run(true);
}

void DriveIntegrationService::AddDriveMountPoint() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZED, state_);
  DCHECK(enabled_);

  weak_ptr_factory_.InvalidateWeakPtrs();

  if (drivefs_holder_ && !drivefs_holder_->drivefs_host()->IsMounted()) {
    PrefService* prefs = profile_->GetPrefs();
    bool was_ever_mounted =
        prefs->GetBoolean(prefs::kDriveFsWasLaunchedAtLeastOnce);
    if (mount_start_.is_null() || was_ever_mounted) {
      mount_start_ = base::TimeTicks::Now();
    }
    drivefs_holder_->drivefs_host()->Mount();
  } else {
    AddDriveMountPointAfterMounted();
  }
}

bool DriveIntegrationService::AddDriveMountPointAfterMounted() {
  const base::FilePath& drive_mount_point = GetMountPointPath();
  if (mount_point_name_.empty()) {
    mount_point_name_ = drive_mount_point.BaseName().AsUTF8Unsafe();
  }
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);
  drivefs_consecutive_failures_count_ = 0;

  bool success = mount_points->RegisterFileSystem(
      mount_point_name_,
      drivefs_holder_ ? storage::kFileSystemTypeDriveFs
                      : storage::kFileSystemTypeDrive,
      storage::FileSystemMountOption(), drive_mount_point);

  if (success) {
    logger_->Log(logging::LOG_INFO, "Drive mount point is added");
    for (auto& observer : observers_)
      observer.OnFileSystemMounted();
  }

  if (drivefs_holder_ && preference_watcher_) {
    preference_watcher_->UpdateSyncPauseState();
  }
  if (drivefs_holder_ &&
      !profile_->GetPrefs()->GetBoolean(prefs::kDriveFsPinnedMigrated)) {
    MigratePinnedFiles();
  }

  return success;
}

void DriveIntegrationService::RemoveDriveMountPoint() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  weak_ptr_factory_.InvalidateWeakPtrs();
  remount_when_online_ = false;

  if (!mount_point_name_.empty()) {
    if (!drivefs_holder_)
      job_list()->CancelAllJobs();

    if (storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
            mount_point_name_)) {
      for (auto& observer : observers_) {
        observer.OnFileSystemBeingUnmounted();
      }
      logger_->Log(logging::LOG_INFO, "Drive mount point is removed");
    }
  }
  if (drivefs_holder_)
    drivefs_holder_->drivefs_host()->Unmount();
}

void DriveIntegrationService::MaybeRemountFileSystem(
    base::Optional<base::TimeDelta> remount_delay,
    bool failed_to_mount) {
  DCHECK_EQ(INITIALIZED, state_);

  RemoveDriveMountPoint();

  if (!remount_delay) {
    if (failed_to_mount && preference_watcher_ &&
        preference_watcher_->is_offline()) {
      logger_->Log(logging::LOG_WARNING,
                   "DriveFs failed to start, will retry when online.");
      remount_when_online_ = true;
      return;
    }
    // If DriveFs didn't specify retry time it's likely unexpected error, e.g.
    // crash. Use limited exponential backoff for retry.
    ++drivefs_consecutive_failures_count_;
    ++drivefs_total_failures_count_;
    if (drivefs_total_failures_count_ > 10) {
      mount_failed_ = true;
      logger_->Log(logging::LOG_ERROR,
                   "DriveFs is too crashy. Leaving it alone.");
      for (auto& observer : observers_)
        observer.OnFileSystemMountFailed();
      return;
    }
    if (drivefs_consecutive_failures_count_ > 3) {
      mount_failed_ = true;
      logger_->Log(logging::LOG_ERROR,
                   "DriveFs keeps failing at start. Giving up.");
      for (auto& observer : observers_)
        observer.OnFileSystemMountFailed();
      return;
    }
    remount_delay = base::TimeDelta::FromSeconds(
        5 * (1 << (drivefs_consecutive_failures_count_ - 1)));
    logger_->Log(logging::LOG_WARNING, "DriveFs died, retry in %d seconds",
                 static_cast<int>(remount_delay.value().InSeconds()));
  }

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DriveIntegrationService::AddDriveMountPoint,
                     weak_ptr_factory_.GetWeakPtr()),
      remount_delay.value());
}

void DriveIntegrationService::OnMounted(const base::FilePath& mount_path) {
  if (AddDriveMountPointAfterMounted()) {
    PrefService* prefs = profile_->GetPrefs();
    bool was_ever_mounted =
        prefs->GetBoolean(prefs::kDriveFsWasLaunchedAtLeastOnce);
    if (was_ever_mounted) {
      UmaEmitMountOutcome(DriveMountStatus::kSuccess, mount_start_);
    } else {
      UmaEmitFirstLaunch(mount_start_);
      prefs->SetBoolean(prefs::kDriveFsWasLaunchedAtLeastOnce, true);
    }
  } else {
    UmaEmitMountOutcome(DriveMountStatus::kUnknownFailure, mount_start_);
  }
}

void DriveIntegrationService::OnUnmounted(
    base::Optional<base::TimeDelta> remount_delay) {
  UmaEmitUnmountOutcome(remount_delay ? DriveMountStatus::kTemporaryUnavailable
                                      : DriveMountStatus::kUnknownFailure);
  MaybeRemountFileSystem(remount_delay, false);
}

void DriveIntegrationService::OnMountFailed(
    MountFailure failure,
    base::Optional<base::TimeDelta> remount_delay) {
  PrefService* prefs = profile_->GetPrefs();
  DriveMountStatus status = ConvertMountFailure(failure);
  UmaEmitMountStatus(status);
  bool was_ever_mounted =
      prefs->GetBoolean(prefs::kDriveFsWasLaunchedAtLeastOnce);
  if (was_ever_mounted) {
    UmaEmitMountTime(status, mount_start_);
  } else {
    // We don't record mount time until we mount successfully at least once.
  }
  MaybeRemountFileSystem(remount_delay, true);
}

void DriveIntegrationService::Initialize() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(NOT_INITIALIZED, state_);
  DCHECK(enabled_);

  state_ = INITIALIZING;

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&InitializeMetadata,
                 cache_root_directory_,
                 metadata_storage_.get(),
                 cache_.get(),
                 resource_metadata_.get(),
                 file_manager::util::GetDownloadsFolderForProfile(profile_)),
      base::Bind(&DriveIntegrationService::InitializeAfterMetadataInitialized,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DriveIntegrationService::InitializeAfterMetadataInitialized(
    FileError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZING, state_);

  if (drivefs_holder_) {
    if (error != FILE_ERROR_OK) {
      profile_->GetPrefs()->SetBoolean(prefs::kDriveFsPinnedMigrated, true);
      metadata_storage_.reset();
      blocking_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &CleanupGCacheV1, cache_root_directory_, base::FilePath(),
              std::vector<std::pair<base::FilePath, std::string>>()));
      metadata_storage_.reset();
    }
    state_ = INITIALIZED;
    if (enabled_)
      AddDriveMountPoint();
    return;
  }

  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  drive_service_->Initialize(identity_manager->GetPrimaryAccountId());

  if (error != FILE_ERROR_OK) {
    LOG(WARNING) << "Failed to initialize: " << FileErrorToString(error);

    // Cannot used Drive. Set the download destination preference out of Drive.
    AvoidDriveAsDownloadDirectoryPreference();

    // Back to NOT_INITIALIZED state. Then, re-running Initialize() should
    // work if the error is recoverable manually (such as out of disk space).
    state_ = NOT_INITIALIZED;
    return;
  }

  // Reset the pref so any migration to DriveFS is performed again the next time
  // DriveFS is enabled. This is necessary to ensure any newly-pinned files are
  // migrated and any dirty files are recovered whenever switching to DriveFS.
  profile_->GetPrefs()->ClearPref(prefs::kDriveFsPinnedMigrated);

  // Initialize Download Handler for hooking downloads to the Drive folder.
  content::DownloadManager* download_manager =
      g_browser_process->download_status_updater()
          ? BrowserContext::GetDownloadManager(profile_)
          : nullptr;
  download_handler_->Initialize(
      download_manager,
      cache_root_directory_.Append(kTemporaryFileDirectory));

  // Install the handler also to incognito profile.
  if (g_browser_process->download_status_updater()) {
    if (profile_->HasOffTheRecordProfile()) {
      download_handler_->ObserveIncognitoDownloadManager(
          BrowserContext::GetDownloadManager(
              profile_->GetOffTheRecordProfile()));
    }
    profile_notification_registrar_ =
        std::make_unique<content::NotificationRegistrar>();
    profile_notification_registrar_->Add(
        this,
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::NotificationService::AllSources());
  }

  // Register for Google Drive invalidation notifications.
  DriveNotificationManager* drive_notification_manager =
      DriveNotificationManagerFactory::GetForBrowserContext(profile_);
  if (drive_notification_manager) {
    drive_notification_manager->AddObserver(this);
    const bool registered =
        drive_notification_manager->push_notification_registered();
    const char* status = (registered ? "registered" : "not registered");
    logger_->Log(logging::LOG_INFO, "Push notification is %s", status);
  }

  state_ = INITIALIZED;

  // Mount only when the drive is enabled. Initialize is triggered by
  // SetEnabled(true), but there is a change to disable it again during
  // the metadata initialization, so we need to look this up again here.
  if (enabled_)
    AddDriveMountPoint();
}

void DriveIntegrationService::AvoidDriveAsDownloadDirectoryPreference() {
  if (DownloadDirectoryPreferenceIsInDrive()) {
    profile_->GetPrefs()->SetFilePath(
        ::prefs::kDownloadDefaultDirectory,
        file_manager::util::GetDownloadsFolderForProfile(profile_));
  }
}

bool DriveIntegrationService::DownloadDirectoryPreferenceIsInDrive() {
  const auto downloads_path =
      profile_->GetPrefs()->GetFilePath(::prefs::kDownloadDefaultDirectory);

  if (!drivefs_holder_) {
    return util::IsUnderDriveMountPoint(downloads_path);
  }
  const auto* user = chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  return user && user->GetAccountId().HasAccountIdKey() &&
         GetMountPointPath().IsParent(downloads_path);
}

void DriveIntegrationService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_CREATED, type);
  Profile* created_profile = content::Source<Profile>(source).ptr();
  if (created_profile->IsOffTheRecord() &&
      created_profile->IsSameProfile(profile_)) {
    download_handler_->ObserveIncognitoDownloadManager(
        BrowserContext::GetDownloadManager(created_profile));
  }
}

void DriveIntegrationService::MigratePinnedFiles() {
  if (!metadata_storage_)
    return;

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &GetPinnedFiles, std::move(metadata_storage_), cache_root_directory_,
          file_manager::util::GetDownloadsFolderForProfile(profile_)),
      base::BindOnce(&DriveIntegrationService::PinFiles,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveIntegrationService::PinFiles(
    const std::vector<base::FilePath>& files_to_pin) {
  if (!drivefs_holder_->drivefs_host()->IsMounted())
    return;

  for (const auto& path : files_to_pin) {
    GetDriveFsInterface()->SetPinned(path, true, base::DoNothing());
  }
  profile_->GetPrefs()->SetBoolean(prefs::kDriveFsPinnedMigrated, true);
}

//===================== DriveIntegrationServiceFactory =======================

DriveIntegrationServiceFactory::FactoryCallback*
    DriveIntegrationServiceFactory::factory_for_test_ = nullptr;

DriveIntegrationServiceFactory::ScopedFactoryForTest::ScopedFactoryForTest(
    FactoryCallback* factory_for_test) {
  factory_for_test_ = factory_for_test;
}

DriveIntegrationServiceFactory::ScopedFactoryForTest::~ScopedFactoryForTest() {
  factory_for_test_ = nullptr;
}

// static
DriveIntegrationService* DriveIntegrationServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DriveIntegrationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
DriveIntegrationService* DriveIntegrationServiceFactory::FindForProfile(
    Profile* profile) {
  return static_cast<DriveIntegrationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
DriveIntegrationServiceFactory* DriveIntegrationServiceFactory::GetInstance() {
  return base::Singleton<DriveIntegrationServiceFactory>::get();
}

DriveIntegrationServiceFactory::DriveIntegrationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DriveIntegrationService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(DriveNotificationManagerFactory::GetInstance());
  DependsOn(DownloadCoreServiceFactory::GetInstance());
}

DriveIntegrationServiceFactory::~DriveIntegrationServiceFactory() = default;

content::BrowserContext* DriveIntegrationServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* DriveIntegrationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  DriveIntegrationService* service = nullptr;
  if (!factory_for_test_) {
    DriveIntegrationService::PreferenceWatcher* preference_watcher = nullptr;
    if (chromeos::IsProfileAssociatedWithGaiaAccount(profile)) {
      // Drive File System can be enabled.
      preference_watcher =
          new DriveIntegrationService::PreferenceWatcher(profile->GetPrefs());
    }

    service =
        new DriveIntegrationService(profile, preference_watcher, nullptr,
                                    std::string(), base::FilePath(), nullptr);
  } else {
    service = factory_for_test_->Run(profile);
  }

  return service;
}

}  // namespace drive
