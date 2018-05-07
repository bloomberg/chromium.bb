// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_HOST_H_
#define CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/account_id/account_id.h"

namespace drivefs {

// A host for a DriveFS process. In addition to managing its lifetime via
// mounting and unmounting, it also bridges between the DriveFS process and the
// file manager.
class COMPONENT_EXPORT(DRIVEFS) DriveFsHost
    : public mojom::DriveFsDelegate,
      public chromeos::disks::DiskMountManager::Observer {
 public:
  class MojoConnectionDelegate {
   public:
    virtual ~MojoConnectionDelegate() = default;

    // Prepare the mojo connection to be used to communicate with the DriveFS
    // process. Returns the mojo handle to use for bootstrapping.
    virtual mojom::DriveFsBootstrapPtrInfo InitializeMojoConnection() = 0;

    // Accepts the mojo connection over |handle|.
    virtual void AcceptMojoConnection(base::ScopedFD handle) = 0;
  };

  DriveFsHost(const base::FilePath& profile_path,
              const AccountId& account,
              base::RepeatingCallback<std::unique_ptr<MojoConnectionDelegate>()>
                  mojo_connection_delegate_factory = {});
  ~DriveFsHost() override;

  // Mount DriveFS.
  bool Mount();

  // Unmount DriveFS.
  void Unmount();

 private:
  class MountState;

  // mojom::DriveFsDelegate:
  void GetAccessToken(const std::string& client_id,
                      const std::string& app_id,
                      const std::vector<std::string>& scopes,
                      GetAccessTokenCallback callback) override;

  // DiskMountManager::Observer:
  void OnMountEvent(chromeos::disks::DiskMountManager::MountEvent event,
                    chromeos::MountError error_code,
                    const chromeos::disks::DiskMountManager::MountPointInfo&
                        mount_info) override;

  // The path to the user's profile.
  const base::FilePath profile_path_;

  // The user whose DriveFS instance |this| is host to.
  const AccountId account_id_;

  // A callback used to create mojo connection delegates.
  const base::RepeatingCallback<std::unique_ptr<MojoConnectionDelegate>()>
      mojo_connection_delegate_factory_;

  // State specific to the current mount, or null if not mounted.
  std::unique_ptr<MountState> mount_state_;

  DISALLOW_COPY_AND_ASSIGN(DriveFsHost);
};

}  // namespace drivefs

#endif  // CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_HOST_H_
