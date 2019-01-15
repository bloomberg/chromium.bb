// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/drivefs_host.h"

#include <map>
#include <set>
#include <utility>

#include "base/strings/strcat.h"
#include "base/unguessable_token.h"
#include "chromeos/components/drivefs/drivefs_bootstrap.h"
#include "chromeos/components/drivefs/drivefs_host_observer.h"
#include "chromeos/components/drivefs/drivefs_search.h"
#include "components/drive/drive_notification_manager.h"
#include "components/drive/drive_notification_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "net/base/network_change_notifier.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"

namespace drivefs {

namespace {

constexpr char kMountScheme[] = "drivefs://";
constexpr char kDataPath[] = "GCache/v2";
constexpr base::TimeDelta kMountTimeout = base::TimeDelta::FromSeconds(20);

}  // namespace

std::unique_ptr<DriveFsBootstrapListener>
DriveFsHost::Delegate::CreateMojoListener() {
  return std::make_unique<DriveFsBootstrapListener>();
}

// A container of state tied to a particular mounting of DriveFS. None of this
// should be shared between mounts.
class DriveFsHost::MountState
    : public mojom::DriveFsDelegate,
      public chromeos::disks::DiskMountManager::Observer,
      public drive::DriveNotificationObserver {
 public:
  explicit MountState(DriveFsHost* host)
      : host_(host),
        weak_ptr_factory_(this) {
    host_->disk_mount_manager_->AddObserver(this);

    auto access_token = host_->account_token_delegate_->TakeCachedAccessToken();
    token_fetch_attempted_ = bool{access_token};

    mojo_connection_ = CreateMojoConnection(
        {base::in_place, host_->delegate_->GetAccountId().GetUserEmail(),
         std::move(access_token)});

    auto pending_token = mojo_connection_->pending_token();
    CHECK(pending_token);
    source_path_ = base::StrCat({kMountScheme, pending_token.ToString()});
    std::string datadir_option =
        base::StrCat({"datadir=", host_->GetDataPath().value()});

    host_->disk_mount_manager_->MountPath(
        source_path_, "",
        base::StrCat({"drivefs-", host_->delegate_->GetObfuscatedAccountId()}),
        {datadir_option}, chromeos::MOUNT_TYPE_NETWORK_STORAGE,
        chromeos::MOUNT_ACCESS_MODE_READ_WRITE);

    host_->timer_->Start(
        FROM_HERE, kMountTimeout,
        base::BindOnce(&MountState::OnTimedOut, base::Unretained(this)));

    search_ =
        std::make_unique<DriveFsSearch>(GetDriveFsInterface(), host_->clock_);
  }

  ~MountState() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    host_->disk_mount_manager_->RemoveObserver(this);
    host_->delegate_->GetDriveNotificationManager().RemoveObserver(this);
    host_->timer_->Stop();
    if (!mount_path_.empty()) {
      host_->disk_mount_manager_->UnmountPath(
          mount_path_.value(), chromeos::UNMOUNT_OPTIONS_NONE, {});
    }
    if (mounted()) {
      for (auto& observer : host_->observers_) {
        observer.OnUnmounted();
      }
    }
  }

  bool mounted() const { return drivefs_has_mounted_ && !mount_path_.empty(); }
  const base::FilePath& mount_path() const { return mount_path_; }

  mojom::DriveFs* GetDriveFsInterface() const {
    return mojo_connection_->drivefs_interface();
  }

  mojom::QueryParameters::QuerySource SearchDriveFs(
      mojom::QueryParametersPtr query,
      mojom::SearchQuery::GetNextPageCallback callback) {
    return search_->PerformSearch(std::move(query), std::move(callback));
  }

 private:
  // mojom::DriveFsDelegate:
  void GetAccessToken(const std::string& client_id,
                      const std::string& app_id,
                      const std::vector<std::string>& scopes,
                      GetAccessTokenCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    host_->account_token_delegate_->GetAccessToken(!token_fetch_attempted_,
                                                   std::move(callback));
    token_fetch_attempted_ = true;
  }

  void OnHeartbeat() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    if (host_->timer_->IsRunning()) {
      host_->timer_->Reset();
    }
  }

  void OnMounted() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    DCHECK(!drivefs_has_mounted_);
    drivefs_has_mounted_ = true;
    MaybeNotifyDelegateOnMounted();
  }

  void OnMountFailed(base::Optional<base::TimeDelta> remount_delay) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    drivefs_has_mounted_ = false;
    drivefs_has_terminated_ = true;
    bool needs_restart = remount_delay.has_value();
    host_->mount_observer_->OnMountFailed(
        needs_restart ? MountObserver::MountFailure::kNeedsRestart
                      : MountObserver::MountFailure::kUnknown,
        std::move(remount_delay));
  }

  void OnUnmounted(base::Optional<base::TimeDelta> remount_delay) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    drivefs_has_mounted_ = false;
    drivefs_has_terminated_ = true;
    host_->mount_observer_->OnUnmounted(std::move(remount_delay));
  }

  void OnSyncingStatusUpdate(mojom::SyncingStatusPtr status) override {
    for (auto& observer : host_->observers_) {
      observer.OnSyncingStatusUpdate(*status);
    }
  }

  void OnFilesChanged(std::vector<mojom::FileChangePtr> changes) override {
    std::vector<mojom::FileChange> changes_values;
    changes_values.reserve(changes.size());
    for (auto& change : changes) {
      changes_values.emplace_back(std::move(*change));
    }
    for (auto& observer : host_->observers_) {
      observer.OnFilesChanged(changes_values);
    }
  }

  void OnError(mojom::DriveErrorPtr error) override {
    if (!IsKnownEnumValue(error->type)) {
      return;
    }
    for (auto& observer : host_->observers_) {
      observer.OnError(*error);
    }
  }

  void OnTeamDrivesListReady(
      const std::vector<std::string>& team_drive_ids) override {
    host_->delegate_->GetDriveNotificationManager().AddObserver(this);
    host_->delegate_->GetDriveNotificationManager().UpdateTeamDriveIds(
        std::set<std::string>(team_drive_ids.begin(), team_drive_ids.end()),
        {});
  }

  void OnTeamDriveChanged(const std::string& team_drive_id,
                          CreateOrDelete change_type) override {
    std::set<std::string> additions;
    std::set<std::string> removals;
    if (change_type == mojom::DriveFsDelegate::CreateOrDelete::kCreated) {
      additions.insert(team_drive_id);
    } else {
      removals.insert(team_drive_id);
    }
    host_->delegate_->GetDriveNotificationManager().UpdateTeamDriveIds(
        additions, removals);
  }

  void OnConnectionError() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    if (!drivefs_has_terminated_) {
      if (mounted()) {
        host_->mount_observer_->OnUnmounted({});
      } else {
        host_->mount_observer_->OnMountFailed(
            MountObserver::MountFailure::kIpcDisconnect, {});
      }
      drivefs_has_terminated_ = true;
    }
  }

  void OnTimedOut() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    host_->timer_->Stop();
    drivefs_has_mounted_ = false;
    drivefs_has_terminated_ = true;
    host_->mount_observer_->OnMountFailed(MountObserver::MountFailure::kTimeout,
                                          {});
  }

  void MaybeNotifyDelegateOnMounted() {
    if (mounted()) {
      host_->timer_->Stop();
      host_->mount_observer_->OnMounted(mount_path());
    }
  }

  // DiskMountManager::Observer:
  void OnMountEvent(chromeos::disks::DiskMountManager::MountEvent event,
                    chromeos::MountError error_code,
                    const chromeos::disks::DiskMountManager::MountPointInfo&
                        mount_info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    if (mount_info.mount_type != chromeos::MOUNT_TYPE_NETWORK_STORAGE ||
        mount_info.source_path != source_path_ ||
        event != chromeos::disks::DiskMountManager::MOUNTING) {
      return;
    }
    if (error_code != chromeos::MOUNT_ERROR_NONE) {
      auto* observer = host_->mount_observer_;

      // Deletes |this|.
      host_->Unmount();
      observer->OnMountFailed(MountObserver::MountFailure::kInvocation, {});
      return;
    }
    DCHECK(!mount_info.mount_path.empty());
    mount_path_ = base::FilePath(mount_info.mount_path);
    host_->disk_mount_manager_->RemoveObserver(this);
    MaybeNotifyDelegateOnMounted();
  }

  // DriveNotificationObserver overrides:
  void OnNotificationReceived(
      const std::map<std::string, int64_t>& invalidations) override {
    std::vector<mojom::FetchChangeLogOptionsPtr> options;
    options.reserve(invalidations.size());
    for (const auto& invalidation : invalidations) {
      options.emplace_back(base::in_place, invalidation.second,
                           invalidation.first);
    }
    GetDriveFsInterface()->FetchChangeLog(std::move(options));
  }

  void OnNotificationTimerFired() override {
    GetDriveFsInterface()->FetchAllChangeLogs();
  }

  std::unique_ptr<DriveFsConnection> CreateMojoConnection(
      mojom::DriveFsConfigurationPtr config) {
    return std::make_unique<DriveFsConnection>(
        host_->delegate_->CreateMojoListener(), std::move(config), this,
        base::BindOnce(&MountState::OnConnectionError, base::Unretained(this)));
  }

  // Owns |this|.
  DriveFsHost* const host_;

  std::unique_ptr<DriveFsConnection> mojo_connection_;

  // The path passed to cros-disks to mount.
  std::string source_path_;

  // The path where DriveFS is mounted.
  base::FilePath mount_path_;

  std::unique_ptr<DriveFsSearch> search_;

  bool drivefs_has_mounted_ = false;
  bool drivefs_has_terminated_ = false;
  bool token_fetch_attempted_ = false;

  base::Time last_shared_with_me_response_;

  base::WeakPtrFactory<MountState> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MountState);
};

DriveFsHost::DriveFsHost(const base::FilePath& profile_path,
                         DriveFsHost::Delegate* delegate,
                         DriveFsHost::MountObserver* mount_observer,
                         const base::Clock* clock,
                         chromeos::disks::DiskMountManager* disk_mount_manager,
                         std::unique_ptr<base::OneShotTimer> timer)
    : profile_path_(profile_path),
      delegate_(delegate),
      mount_observer_(mount_observer),
      clock_(clock),
      disk_mount_manager_(disk_mount_manager),
      timer_(std::move(timer)),
      account_token_delegate_(
          std::make_unique<DriveFsAuth>(clock, profile_path, delegate)) {
  DCHECK(delegate_);
  DCHECK(mount_observer_);
  DCHECK(clock_);
}

DriveFsHost::~DriveFsHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DriveFsHost::AddObserver(DriveFsHostObserver* observer) {
  observers_.AddObserver(observer);
}

void DriveFsHost::RemoveObserver(DriveFsHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool DriveFsHost::Mount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const AccountId& account_id = delegate_->GetAccountId();
  if (mount_state_ || !account_id.HasAccountIdKey() ||
      account_id.GetUserEmail().empty()) {
    return false;
  }
  mount_state_ = std::make_unique<MountState>(this);
  return true;
}

void DriveFsHost::Unmount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mount_state_.reset();
}

bool DriveFsHost::IsMounted() const {
  return mount_state_ && mount_state_->mounted();
}

base::FilePath DriveFsHost::GetMountPath() const {
  return mount_state_ && mount_state_->mounted()
             ? mount_state_->mount_path()
             : base::FilePath("/media/fuse")
                   .Append(base::StrCat(
                       {"drivefs-", delegate_->GetObfuscatedAccountId()}));
}

base::FilePath DriveFsHost::GetDataPath() const {
  return profile_path_.Append(kDataPath).Append(
      delegate_->GetObfuscatedAccountId());
}

mojom::DriveFs* DriveFsHost::GetDriveFsInterface() const {
  if (!mount_state_ || !mount_state_->mounted()) {
    return nullptr;
  }
  return mount_state_->GetDriveFsInterface();
}

mojom::QueryParameters::QuerySource DriveFsHost::PerformSearch(
    mojom::QueryParametersPtr query,
    mojom::SearchQuery::GetNextPageCallback callback) {
  return mount_state_->SearchDriveFs(std::move(query), std::move(callback));
}

}  // namespace drivefs
