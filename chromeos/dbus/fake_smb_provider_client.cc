// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_smb_provider_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeSmbProviderClient::FakeSmbProviderClient() {}

FakeSmbProviderClient::~FakeSmbProviderClient() {}

void FakeSmbProviderClient::Init(dbus::Bus* bus) {}

void FakeSmbProviderClient::Mount(const std::string& share_path,
                                  MountCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK, 1));
}

void FakeSmbProviderClient::Unmount(int32_t mount_id,
                                    UnmountCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK));
}

}  // namespace chromeos
