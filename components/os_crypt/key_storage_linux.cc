// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_linux.h"

#include "components/os_crypt/key_storage_libsecret.h"

// static
std::unique_ptr<KeyStorageLinux> KeyStorageLinux::CreateService() {
  std::unique_ptr<KeyStorageLinux> key_storage(new KeyStorageLibsecret());

  if (key_storage->Init())
    return key_storage;

  return nullptr;
}
