// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_service.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/smb_client/smb_file_system.h"
#include "chrome/browser/chromeos/smb_client/smb_file_system_id.h"
#include "chrome/browser/chromeos/smb_client/smb_provider.h"
#include "chrome/browser/chromeos/smb_client/smb_service_factory.h"
#include "chrome/browser/chromeos/smb_client/temp_file_manager.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/smb_provider_client.h"

using chromeos::file_system_provider::Service;

namespace chromeos {
namespace smb_client {

SmbService::SmbService(Profile* profile)
    : provider_id_(ProviderId::CreateFromNativeId("smb")), profile_(profile) {
  if (base::FeatureList::IsEnabled(features::kNativeSmb)) {
    GetProviderService()->RegisterProvider(std::make_unique<SmbProvider>(
        base::BindRepeating(&SmbService::Unmount, base::Unretained(this))));
    RestoreMounts();
  }
}

SmbService::~SmbService() {}

// static
SmbService* SmbService::Get(content::BrowserContext* context) {
  return SmbServiceFactory::Get(context);
}

void SmbService::Mount(const file_system_provider::MountOptions& options,
                       const base::FilePath& share_path,
                       MountResponse callback) {
  if (!temp_file_manager_) {
    InitTempFileManagerAndMount(options, share_path, std::move(callback));
    return;
  }

  CallMount(options, share_path, std::move(callback));
}

void SmbService::InitTempFileManagerAndMount(
    const file_system_provider::MountOptions& options,
    const base::FilePath& share_path,
    MountResponse callback) {
  // InitTempFileManager() has to be called on a separate thread since it
  // contains a call that requires a blockable thread.
  base::TaskTraits task_traits = {base::MayBlock(),
                                  base::TaskPriority::USER_BLOCKING,
                                  base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};
  base::OnceClosure task =
      base::BindOnce(&SmbService::InitTempFileManager, base::Unretained(this));
  base::OnceClosure reply =
      base::BindOnce(&SmbService::CallMount, base::Unretained(this), options,
                     share_path, base::Passed(&callback));

  base::PostTaskWithTraitsAndReply(FROM_HERE, task_traits, std::move(task),
                                   std::move(reply));
}

void SmbService::CallMount(const file_system_provider::MountOptions& options,
                           const base::FilePath& share_path,
                           MountResponse callback) {
  // TODO(allenvic): Implement passing of credentials. This currently passes
  // empty credentials to SmbProvider.
  GetSmbProviderClient()->Mount(
      share_path, "" /* workgroup */, "" /* username */,
      temp_file_manager_->WritePasswordToFile("" /* password */),
      base::BindOnce(&SmbService::OnMountResponse, AsWeakPtr(),
                     base::Passed(&callback), options, share_path));
}

void SmbService::OnMountResponse(
    MountResponse callback,
    const file_system_provider::MountOptions& options,
    const base::FilePath& share_path,
    smbprovider::ErrorType error,
    int32_t mount_id) {
  if (error != smbprovider::ERROR_OK) {
    std::move(callback).Run(SmbFileSystem::TranslateError(error));
    return;
  }

  DCHECK_GE(mount_id, 0);

  file_system_provider::MountOptions mount_options(options);
  mount_options.file_system_id = CreateFileSystemId(mount_id, share_path);

  base::File::Error result =
      GetProviderService()->MountFileSystem(provider_id_, mount_options);

  std::move(callback).Run(result);
}

base::File::Error SmbService::Unmount(
    const std::string& file_system_id,
    file_system_provider::Service::UnmountReason reason) const {
  return GetProviderService()->UnmountFileSystem(provider_id_, file_system_id,
                                                 reason);
}

Service* SmbService::GetProviderService() const {
  return file_system_provider::Service::Get(profile_);
}

SmbProviderClient* SmbService::GetSmbProviderClient() const {
  return chromeos::DBusThreadManager::Get()->GetSmbProviderClient();
}

void SmbService::RestoreMounts() {
  const std::vector<ProvidedFileSystemInfo> file_systems =
      GetProviderService()->GetProvidedFileSystemInfoList(provider_id_);

  for (const auto& file_system : file_systems) {
    Remount(file_system);
  }
}

void SmbService::Remount(const ProvidedFileSystemInfo& file_system_info) {
  const base::FilePath share_path =
      GetSharePathFromFileSystemId(file_system_info.file_system_id());
  const int32_t mount_id =
      GetMountIdFromFileSystemId(file_system_info.file_system_id());
  GetSmbProviderClient()->Remount(
      share_path, mount_id,
      base::BindOnce(&SmbService::OnRemountResponse, AsWeakPtr(),
                     file_system_info.file_system_id()));
}

void SmbService::OnRemountResponse(const std::string& file_system_id,
                                   smbprovider::ErrorType error) {
  if (error != smbprovider::ERROR_OK) {
    LOG(ERROR) << "SmbService: failed to restore filesystem: "
               << file_system_id;
    Unmount(file_system_id, file_system_provider::Service::UNMOUNT_REASON_USER);
  }
}

void SmbService::InitTempFileManager() {
  temp_file_manager_ = std::make_unique<TempFileManager>();
}

}  // namespace smb_client
}  // namespace chromeos
