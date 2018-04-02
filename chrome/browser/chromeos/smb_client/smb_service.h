// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provider_interface.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/smb_provider_client.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {
namespace smb_client {

using file_system_provider::Capabilities;
using file_system_provider::ProvidedFileSystemInfo;
using file_system_provider::ProvidedFileSystemInterface;
using file_system_provider::ProviderId;
using file_system_provider::ProviderInterface;
using file_system_provider::Service;

// Creates and manages an smb file system.
class SmbService : public KeyedService,
                   public base::SupportsWeakPtr<SmbService> {
 public:
  using MountResponse = base::OnceCallback<void(base::File::Error error)>;

  explicit SmbService(Profile* profile);
  ~SmbService() override;

  // Gets the singleton instance for the |context|.
  static SmbService* Get(content::BrowserContext* context);

  // Starts the process of mounting an SMB file system.
  // Calls SmbProviderClient::Mount().
  void Mount(const file_system_provider::MountOptions& options,
             const base::FilePath& share_path,
             MountResponse callback);

  // Completes the mounting of an SMB file system, passing |options| on to
  // file_system_provider::Service::MountFileSystem(). Passes error status to
  // callback.
  void OnMountResponse(MountResponse callback,
                       const file_system_provider::MountOptions& options,
                       smbprovider::ErrorType error,
                       int32_t mount_id);

 private:
  // Calls file_system_provider::Service::UnmountFileSystem().
  base::File::Error Unmount(
      const std::string& file_system_id,
      file_system_provider::Service::UnmountReason reason) const;

  Service* GetProviderService() const;

  SmbProviderClient* GetSmbProviderClient() const;

  const ProviderId provider_id_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SmbService);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_
