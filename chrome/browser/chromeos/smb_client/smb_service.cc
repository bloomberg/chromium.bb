// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_service.h"

#include "chrome/browser/chromeos/smb_client/smb_file_system.h"

using chromeos::file_system_provider::Service;

namespace chromeos {
namespace smb_client {
namespace {

const char kSmbProviderId[] = "smb";

using file_system_provider::ProvidedFileSystemInterface;

// Factory for smb file systems. |profile| must not be NULL.
std::unique_ptr<ProvidedFileSystemInterface> CreateSmbFileSystem(
    Profile* profile,
    const file_system_provider::ProvidedFileSystemInfo& file_system_info) {
  DCHECK(profile);
  return base::MakeUnique<SmbFileSystem>(file_system_info);
}

}  // namespace

SmbService::SmbService(Profile* profile) : profile_(profile) {
  GetProviderService()->RegisterFileSystemFactory(
      kSmbProviderId, base::Bind(&CreateSmbFileSystem));
}

SmbService::~SmbService() {}

base::File::Error SmbService::Mount(
    const file_system_provider::MountOptions& options) {
  return GetProviderService()->MountFileSystem(kSmbProviderId, options);
}

Service* SmbService::GetProviderService() const {
  return file_system_provider::Service::Get(profile_);
}

}  // namespace smb_client
}  // namespace chromeos
