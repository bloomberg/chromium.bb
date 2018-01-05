// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_smb_provider_client.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeSmbProviderClient::FakeSmbProviderClient() {}

FakeSmbProviderClient::~FakeSmbProviderClient() {}

void FakeSmbProviderClient::Init(dbus::Bus* bus) {}

void FakeSmbProviderClient::Mount(const base::FilePath& share_path,
                                  MountCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK, 1));
}

void FakeSmbProviderClient::Unmount(int32_t mount_id,
                                    UnmountCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK));
}

void FakeSmbProviderClient::ReadDirectory(int32_t mount_id,
                                          const base::FilePath& directory_path,
                                          ReadDirectoryCallback callback) {
  smbprovider::DirectoryEntryList entry_list;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), smbprovider::ERROR_OK, entry_list));
}

void FakeSmbProviderClient::GetMetadataEntry(int32_t mount_id,
                                             const base::FilePath& entry_path,
                                             GetMetdataEntryCallback callback) {
  smbprovider::DirectoryEntry entry;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), smbprovider::ERROR_OK, entry));
}

void FakeSmbProviderClient::OpenFile(int32_t mount_id,
                                     const base::FilePath& file_path,
                                     bool writeable,
                                     OpenFileCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK, 1));
}

void FakeSmbProviderClient::CloseFile(int32_t mount_id,
                                      int32_t file_id,
                                      CloseFileCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK));
}

}  // namespace chromeos
