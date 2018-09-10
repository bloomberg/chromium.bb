// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ENCRYPTION_KEY_CREATION_UTIL_H_
#define COMPONENTS_OS_CRYPT_ENCRYPTION_KEY_CREATION_UTIL_H_

#include "base/component_export.h"

namespace os_crypt {

// An interface for the utility that prevents overwriting the encryption key on
// Mac.
// This class is used on Mac and iOS, but does nothing on iOS as the feature
// for preventing key overwrites is available only on Mac. The object for
// the Mac class has to be created on the main thread.
class EncryptionKeyCreationUtil {
 public:
  virtual ~EncryptionKeyCreationUtil() = default;

  // Returns true iff the key should already exists on Mac and false on iOS.
  // This method doesn't need to be called from the main thread.
  virtual bool KeyAlreadyCreated() = 0;

  // Returns true iff the feature for preventing key overwrites is enabled on
  // Mac and false on iOS. This method doesn't need to be called from the main
  // thread.
  virtual bool ShouldPreventOverwriting() = 0;

  // This asynchronously updates the preference on the main thread that the key
  // was created. This method is called when key is added to the Keychain, or
  // the first time the key is successfully retrieved from the Keychain and the
  // preference hasn't been set yet. This method doesn't need to be called on
  // the main thread.
  virtual void OnKeyWasStored() = 0;
};

}  // namespace os_crypt

#endif  // COMPONENTS_OS_CRYPT_ENCRYPTION_KEY_CREATION_UTIL_H_
