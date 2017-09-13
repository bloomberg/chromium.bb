// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_VIRTUAL_FILE_PROVIDER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_VIRTUAL_FILE_PROVIDER_CLIENT_H_

#include "chromeos/dbus/virtual_file_provider_client.h"

namespace chromeos {

class CHROMEOS_EXPORT FakeVirtualFileProviderClient
    : public VirtualFileProviderClient {
 public:
  FakeVirtualFileProviderClient();
  ~FakeVirtualFileProviderClient() override;

  // DBusClient override.
  void Init(dbus::Bus* bus) override;

  // VirtualFileProviderClient overrides:
  void OpenFile(int64_t size, OpenFileCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeVirtualFileProviderClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_VIRTUAL_FILE_PROVIDER_CLIENT_H_
