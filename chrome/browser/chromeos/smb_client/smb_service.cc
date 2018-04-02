// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_service.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/smb_client/smb_file_system.h"
#include "chrome/browser/chromeos/smb_client/smb_provider.h"
#include "chrome/browser/chromeos/smb_client/smb_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/smb_provider_client.h"

using chromeos::file_system_provider::Service;

namespace chromeos {
namespace smb_client {

SmbService::SmbService(Profile* profile)
    : provider_id_(ProviderId::CreateFromNativeId("smb")), profile_(profile) {
  if (base::FeatureList::IsEnabled(features::kNativeSmb))
    GetProviderService()->RegisterProvider(std::make_unique<SmbProvider>(
        base::BindRepeating(&SmbService::Unmount, base::Unretained(this))));
}

SmbService::~SmbService() {}

// static
SmbService* SmbService::Get(content::BrowserContext* context) {
  return SmbServiceFactory::Get(context);
}

void SmbService::Mount(const file_system_provider::MountOptions& options,
                       const base::FilePath& share_path,
                       MountResponse callback) {
  GetSmbProviderClient()->Mount(
      share_path, base::BindOnce(&SmbService::OnMountResponse, AsWeakPtr(),
                                 base::Passed(&callback), options));
}

void SmbService::OnMountResponse(
    MountResponse callback,
    const file_system_provider::MountOptions& options,
    smbprovider::ErrorType error,
    int32_t mount_id) {
  if (error != smbprovider::ERROR_OK) {
    std::move(callback).Run(SmbFileSystem::TranslateError(error));
    return;
  }

  DCHECK_GE(mount_id, 0);

  file_system_provider::MountOptions mount_options(options);
  mount_options.file_system_id = base::NumberToString(mount_id);

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

}  // namespace smb_client
}  // namespace chromeos
