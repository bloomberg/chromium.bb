// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_service.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/smb_client/smb_file_system.h"
#include "chrome/browser/chromeos/smb_client/smb_service_factory.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/smb_provider_client.h"

using chromeos::file_system_provider::Service;

namespace chromeos {
namespace smb_client {

SmbService::SmbService(Profile* profile)
    : profile_(profile),
      provider_id_(ProviderId::CreateFromNativeId("smb")),
      capabilities_(false, false, false, extensions::SOURCE_NETWORK),
      // TODO(baileyberro): Localize this string, so it shows correctly in all
      // languages. See l10n_util::GetStringUTF8.
      name_("SMB Shares"),
      weak_ptr_factory_(this) {
  GetProviderService()->RegisterProvider(std::make_unique<SmbService>(profile));
}

SmbService::~SmbService() {}

// static
SmbService* SmbService::Get(content::BrowserContext* context) {
  return SmbServiceFactory::Get(context);
}

void SmbService::Mount(const file_system_provider::MountOptions& options,
                       const std::string& share_path,
                       MountResponse callback) {
  chromeos::DBusThreadManager::Get()->GetSmbProviderClient()->Mount(
      share_path, base::BindOnce(&SmbService::OnMountResponse,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 base::Passed(&callback), options));
}

void SmbService::OnMountResponse(
    MountResponse callback,
    const file_system_provider::MountOptions& options,
    smbprovider::ErrorType error,
    int32_t mount_id) {
  if (error != smbprovider::ERROR_OK) {
    DCHECK(mount_id >= 0);
    std::move(callback).Run(SmbFileSystem::TranslateError(error));
    return;
  }

  file_system_provider::MountOptions mount_options(options);
  mount_options.file_system_id = base::NumberToString(mount_id);

  base::File::Error providerServiceMountResult =
      GetProviderService()->MountFileSystem(GetId(), mount_options);

  std::move(callback).Run(providerServiceMountResult);
}

Service* SmbService::GetProviderService() const {
  return file_system_provider::Service::Get(profile_);
}

std::unique_ptr<ProvidedFileSystemInterface>
SmbService::CreateProvidedFileSystem(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info) {
  DCHECK(profile);
  return std::make_unique<SmbFileSystem>(file_system_info);
}

const Capabilities& SmbService::GetCapabilities() const {
  return capabilities_;
}

const ProviderId& SmbService::GetId() const {
  return provider_id_;
}

const std::string& SmbService::GetName() const {
  return name_;
}

}  // namespace smb_client
}  // namespace chromeos
