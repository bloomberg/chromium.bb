// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_

#include "base/files/file.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {
namespace smb_client {

// Creates and manages an smb file system.
class SmbService : public KeyedService {
 public:
  explicit SmbService(Profile* profile);
  ~SmbService() override;

  // Mounts an SMB file system, passing |options| on to
  // file_system_provider::Service::MountFileSystem()
  base::File::Error Mount(const file_system_provider::MountOptions& options);

 private:
  file_system_provider::Service* GetProviderService() const;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SmbService);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_
