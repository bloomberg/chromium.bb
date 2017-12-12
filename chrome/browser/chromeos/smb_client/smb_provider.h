// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_PROVIDER_H_

#include <memory>
#include <string>

#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/provider_interface.h"

class Profile;

namespace chromeos {
namespace smb_client {

using file_system_provider::ProviderId;
using file_system_provider::Capabilities;
using file_system_provider::ProvidedFileSystemInfo;
using file_system_provider::ProviderInterface;
using file_system_provider::ProvidedFileSystemInterface;

class SmbProvider : public ProviderInterface {
 public:
  SmbProvider();
  // ProviderInterface overrides.
  std::unique_ptr<ProvidedFileSystemInterface> CreateProvidedFileSystem(
      Profile* profile,
      const ProvidedFileSystemInfo& file_system_info) override;
  const Capabilities& GetCapabilities() const override;
  const ProviderId& GetId() const override;
  const std::string& GetName() const override;

 private:
  ProviderId provider_id_;
  Capabilities capabilities_;
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(SmbProvider);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_PROVIDER_H_
