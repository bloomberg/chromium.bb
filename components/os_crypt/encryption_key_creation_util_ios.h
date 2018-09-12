// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ENCRYPTION_KEY_CREATION_UTIL_IOS_H_
#define COMPONENTS_OS_CRYPT_ENCRYPTION_KEY_CREATION_UTIL_IOS_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "components/os_crypt/encryption_key_creation_util.h"

namespace os_crypt {

// A key creation utility for iOS which does nothing as there is no feature
// for preventing key overwrites for iOS.
class COMPONENT_EXPORT(OS_CRYPT) EncryptionKeyCreationUtilIOS
    : public EncryptionKeyCreationUtil {
 public:
  EncryptionKeyCreationUtilIOS();
  ~EncryptionKeyCreationUtilIOS() override;

  // Returns false.
  bool KeyAlreadyCreated() override;

  // Returns false.
  bool ShouldPreventOverwriting() override;

  // Does nothing.
  void OnKeyWasStored() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(EncryptionKeyCreationUtilIOS);
};

}  // namespace os_crypt

#endif  // COMPONENTS_OS_CRYPT_ENCRYPTION_KEY_CREATION_UTIL_IOS_H_
