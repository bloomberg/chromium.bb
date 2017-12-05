// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_service.h"

#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/smb_client/smb_file_system.h"

using chromeos::file_system_provider::Service;

namespace chromeos {
namespace smb_client {

SmbService::SmbService(Profile* profile)
    : profile_(profile),
      provider_id_(ProviderId::CreateFromNativeId("smb")),
      capabilities_(false, false, false, extensions::SOURCE_NETWORK) {
  GetProviderService()->RegisterProvider(std::make_unique<SmbService>(profile));
}

SmbService::~SmbService() {}

base::File::Error SmbService::Mount(
    const file_system_provider::MountOptions& options) {
  return GetProviderService()->MountFileSystem(provider_id_, options);
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

}  // namespace smb_client
}  // namespace chromeos
