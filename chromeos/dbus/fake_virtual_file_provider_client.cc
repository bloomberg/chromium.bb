// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_virtual_file_provider_client.h"

#include "base/callback.h"

namespace chromeos {

FakeVirtualFileProviderClient::FakeVirtualFileProviderClient() = default;
FakeVirtualFileProviderClient::~FakeVirtualFileProviderClient() = default;

void FakeVirtualFileProviderClient::Init(dbus::Bus* bus) {}

void FakeVirtualFileProviderClient::OpenFile(int64_t size,
                                             OpenFileCallback callback) {}

}  // namespace chromeos
