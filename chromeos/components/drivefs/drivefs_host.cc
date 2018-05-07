// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/drivefs_host.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/unguessable_token.h"
#include "chromeos/components/drivefs/pending_connection_manager.h"
#include "mojo/edk/embedder/connection_params.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/outgoing_broker_client_invitation.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/embedder/transport_protocol.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace drivefs {

namespace {

constexpr char kMountScheme[] = "drivefs://";
constexpr char kDataPath[] = "GCache/v2";

class MojoConnectionDelegateImpl : public DriveFsHost::MojoConnectionDelegate {
 public:
  MojoConnectionDelegateImpl() = default;

  static std::unique_ptr<DriveFsHost::MojoConnectionDelegate> Create() {
    return std::make_unique<MojoConnectionDelegateImpl>();
  }

  mojom::DriveFsBootstrapPtrInfo InitializeMojoConnection() override {
    return mojom::DriveFsBootstrapPtrInfo(
        invitation_.AttachMessagePipe("drivefs-bootstrap"),
        mojom::DriveFsBootstrap::Version_);
  }

  void AcceptMojoConnection(base::ScopedFD handle) override {
    invitation_.Send(base::kNullProcessHandle,
                     {mojo::edk::TransportProtocol::kLegacy,
                      mojo::edk::ScopedPlatformHandle(
                          mojo::edk::PlatformHandle(handle.release()))});
  }

 private:
  // The underlying mojo connection.
  mojo::edk::OutgoingBrokerClientInvitation invitation_;

  DISALLOW_COPY_AND_ASSIGN(MojoConnectionDelegateImpl);
};

base::RepeatingCallback<std::unique_ptr<DriveFsHost::MojoConnectionDelegate>()>
GetMojoConnectionDelegateFactoryOrDefault(
    base::RepeatingCallback<
        std::unique_ptr<DriveFsHost::MojoConnectionDelegate>()> factory) {
  return factory ? factory
                 : base::BindRepeating(&MojoConnectionDelegateImpl::Create);
}

}  // namespace

// A container of state tied to a particular mounting of DriveFS. None of this
// should be shared between mounts.
class DriveFsHost::MountState {
 public:
  explicit MountState(DriveFsHost* host)
      : host_(host),
        mojo_connection_delegate_(
            host_->mojo_connection_delegate_factory_.Run()),
        pending_token_(base::UnguessableToken::Create()),
        binding_(host_) {
    source_path_ = base::StrCat({kMountScheme, pending_token_.ToString(), "@",
                                 host_->profile_path_.Append(kDataPath)
                                     .Append(host_->account_id_.GetGaiaId())
                                     .value()});

    // TODO(sammc): Switch the mount type once a more appropriate mount type
    // exists.
    chromeos::disks::DiskMountManager::GetInstance()->MountPath(
        source_path_, "", "", chromeos::MOUNT_TYPE_ARCHIVE,
        chromeos::MOUNT_ACCESS_MODE_READ_WRITE);

    auto bootstrap =
        mojo::MakeProxy(mojo_connection_delegate_->InitializeMojoConnection());
    mojom::DriveFsDelegatePtr delegate;
    binding_.Bind(mojo::MakeRequest(&delegate));
    bootstrap->Init({base::in_place, host_->account_id_.GetUserEmail()},
                    mojo::MakeRequest(&drivefs_), std::move(delegate));

    // If unconsumed, the registration is cleaned up when |this| is destructed.
    PendingConnectionManager::Get().ExpectOpenIpcChannel(
        pending_token_,
        base::BindOnce(&DriveFsHost::MountState::AcceptMojoConnection,
                       base::Unretained(this)));
  }

  ~MountState() {
    if (pending_token_) {
      PendingConnectionManager::Get().CancelExpectedOpenIpcChannel(
          pending_token_);
    }
    if (!mount_path_.empty()) {
      chromeos::disks::DiskMountManager::GetInstance()->UnmountPath(
          mount_path_, chromeos::UNMOUNT_OPTIONS_NONE, {});
    }
  }

  // Accepts the mojo connection over |handle|, delegating to
  // |mojo_connection_delegate_|.
  void AcceptMojoConnection(base::ScopedFD handle) {
    DCHECK(pending_token_);
    pending_token_ = {};
    mojo_connection_delegate_->AcceptMojoConnection(std::move(handle));
  }

  // Returns false if there was an error for |source_path_| and thus if |this|
  // should be deleted.
  bool OnMountEvent(
      chromeos::disks::DiskMountManager::MountEvent event,
      chromeos::MountError error_code,
      const chromeos::disks::DiskMountManager::MountPointInfo& mount_info) {
    if (mount_info.source_path != source_path_ ||
        event != chromeos::disks::DiskMountManager::MOUNTING) {
      return true;
    }
    if (error_code != chromeos::MOUNT_ERROR_NONE) {
      return false;
    }
    mount_path_ = mount_info.mount_path;
    return true;
  }

 private:
  // Owns this.
  DriveFsHost* const host_;

  const std::unique_ptr<DriveFsHost::MojoConnectionDelegate>
      mojo_connection_delegate_;

  // The token passed to DriveFS as part of |source_path_| used to match it to
  // this DriveFsHost instance.
  base::UnguessableToken pending_token_;

  // The path passed to cros-disks to mount.
  std::string source_path_;

  // The path where DriveFS is mounted.
  std::string mount_path_;

  // Mojo connections to the DriveFS process.
  mojom::DriveFsPtr drivefs_;
  mojo::Binding<mojom::DriveFsDelegate> binding_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MountState);
};

DriveFsHost::DriveFsHost(
    const base::FilePath& profile_path,
    const AccountId& account_id,
    base::RepeatingCallback<
        std::unique_ptr<DriveFsHost::MojoConnectionDelegate>()>
        mojo_connection_delegate_factory)
    : profile_path_(profile_path),
      account_id_(account_id),
      mojo_connection_delegate_factory_(
          GetMojoConnectionDelegateFactoryOrDefault(
              std::move(mojo_connection_delegate_factory))) {
  chromeos::disks::DiskMountManager::GetInstance()->AddObserver(this);
}

DriveFsHost::~DriveFsHost() {
  chromeos::disks::DiskMountManager::GetInstance()->RemoveObserver(this);
}

bool DriveFsHost::Mount() {
  if (mount_state_ || account_id_.GetAccountType() != AccountType::GOOGLE) {
    return false;
  }
  mount_state_ = std::make_unique<MountState>(this);
  return true;
}

void DriveFsHost::Unmount() {
  mount_state_.reset();
}

void DriveFsHost::GetAccessToken(const std::string& client_id,
                                 const std::string& app_id,
                                 const std::vector<std::string>& scopes,
                                 GetAccessTokenCallback callback) {
  // TODO(crbug.com/823956): Implement this.
  std::move(callback).Run(mojom::AccessTokenStatus::kAuthError, "");
}

void DriveFsHost::OnMountEvent(
    chromeos::disks::DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const chromeos::disks::DiskMountManager::MountPointInfo& mount_info) {
  if (!mount_state_) {
    return;
  }
  if (!mount_state_->OnMountEvent(event, error_code, mount_info)) {
    Unmount();
  }
}

}  // namespace drivefs
