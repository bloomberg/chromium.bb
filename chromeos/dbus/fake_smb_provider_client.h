// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SMB_PROVIDER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SMB_PROVIDER_CLIENT_H_

#include "chromeos/dbus/smb_provider_client.h"

namespace chromeos {

// A fake implementation of SmbProviderClient.
class CHROMEOS_EXPORT FakeSmbProviderClient : public SmbProviderClient {
 public:
  FakeSmbProviderClient();
  ~FakeSmbProviderClient() override;

  // DBusClient override.
  void Init(dbus::Bus* bus) override;

  // SmbProviderClient override.
  void Mount(const base::FilePath& share_path, MountCallback callback) override;
  void Unmount(int32_t mount_id, UnmountCallback callback) override;
  void ReadDirectory(int32_t mount_id,
                     const base::FilePath& directory_path,
                     ReadDirectoryCallback callback) override;
  void GetMetadataEntry(int32_t mount_id,
                        const base::FilePath& entry_path,
                        GetMetdataEntryCallback callback) override;
  void OpenFile(int32_t mount_id,
                const base::FilePath& file_path,
                bool writeable,
                OpenFileCallback callback) override;
  void CloseFile(int32_t mount_id,
                 int32_t file_id,
                 CloseFileCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeSmbProviderClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SMB_PROVIDER_CLIENT_H_
