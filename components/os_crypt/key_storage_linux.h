// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_KEY_STORAGE_LINUX_H_
#define COMPONENTS_OS_CRYPT_KEY_STORAGE_LINUX_H_

#include <memory>
#include <string>

#include "base/macros.h"

namespace os_crypt {
struct Config;
}

// An API for retrieving OSCrypt's password from the system's password storage
// service.
class KeyStorageLinux {
 public:
  KeyStorageLinux() = default;
  virtual ~KeyStorageLinux() = default;

  // Tries to load the appropriate key storage. Returns null if none succeed.
  static std::unique_ptr<KeyStorageLinux> CreateService(
      const os_crypt::Config& config);

  // Gets the encryption key from the OS password-managing library. If a key is
  // not found, a new key will be generated, stored and returned.
  std::string GetKey();

 protected:
  // Loads the key storage. Returns false if the service is not available.
  virtual bool Init() = 0;

  // The implementation of GetKey() for a specific backend. This will be called
  // on the backend's preferred thread.
  virtual std::string GetKeyImpl() = 0;

  // The name of the group, if any, containing the key.
  static const char kFolderName[];
  // The name of the entry with the encryption key.
  static const char kKey[];

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyStorageLinux);
};

#endif  // COMPONENTS_OS_CRYPT_KEY_STORAGE_LINUX_H_
