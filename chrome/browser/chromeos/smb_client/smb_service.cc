// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_service.h"

#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/smb_client/smb_file_system.h"

using chromeos::file_system_provider::Service;

namespace chromeos {
namespace smb_client {

file_system_provider::ProviderId kSmbProviderId =
    ProviderId::CreateFromNativeId("smb");

SmbService::SmbService(Profile* profile) : profile_(profile) {
  GetProviderService()->RegisterNativeProvider(
      kSmbProviderId, std::make_unique<SmbService>(profile));
}

SmbService::~SmbService() {}

base::File::Error SmbService::Mount(
    const file_system_provider::MountOptions& options) {
  return GetProviderService()->MountFileSystem(kSmbProviderId, options);
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

bool SmbService::GetCapabilities(Profile* profile,
                                 const ProviderId& provider_id,
                                 Capabilities& result) {
  result = Capabilities(false, false, false, extensions::SOURCE_NETWORK);
  return true;
}

}  // namespace smb_client
}  // namespace chromeos
