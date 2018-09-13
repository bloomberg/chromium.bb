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
  // The action that is taken by GetPassword method.
  // This enum is used for reporting metrics.
  enum class GetKeyAction {
    // Key was found in the Keychain and the preference that it was created in
    // the past is set.
    kKeyFound = 0,
    // Key was found in the Keychain, but the preference that it was created in
    // the past was not set. This should happen when Chrome is updated to the
    // version supporting the feature for preventing encryption key overwrites.
    kKeyFoundFirstTime = 1,
    // Key couldn't be found in the Keychain, but the preference suggests it
    // was created in the past.
    kOverwritingPrevented = 2,
    // Key was added to the Keychain and the preference is set.
    kNewKeyAddedToKeychain = 3,
    // Some other error occurred.
    kKeychainLookupFailed = 4,
    kMaxValue = kKeychainLookupFailed,
  };

  virtual ~EncryptionKeyCreationUtil() = default;

  // Returns true iff the key should already exists on Mac and false on iOS.
  // This method doesn't need to be called from the main thread.
  virtual bool KeyAlreadyCreated() = 0;

  // Returns true iff the feature for preventing key overwrites is enabled on
  // Mac and false on iOS. This method doesn't need to be called from the main
  // thread.
  virtual bool ShouldPreventOverwriting() = 0;

  // This method is called when the encryption key is successfully retrieved
  // from the Keychain. It resets the counter of how many times Chrome prevented
  // overwriting the key to zero asynchronously on the main thread. If this is
  // called for the very first time, it asynchronously updates the preference
  // on the main thread that the key was created. This method doesn't need to be
  // called on the main thread.
  virtual void OnKeyWasFound() = 0;

  // This method is called when encryption key is added to the Keychain. It
  // asynchronously updates the preference on the main thread that the key was
  // created. This method doesn't need to be called on the main thread.
  virtual void OnKeyWasStored() = 0;

  // Asynchronously increases the number of times overwriting the encryption key
  // was prevented on the main thread. This method doesn't need to be called on
  // the main thread.
  virtual void OnOverwritingPrevented() = 0;

  // This method is called when the Keychain returns error other than
  // errSecItemNotFound (e.g., user is not authorized to use Keychain, or
  // Keychain is unavailable for some other reasons).
  virtual void OnKeychainLookupFailed() = 0;
};

}  // namespace os_crypt

#endif  // COMPONENTS_OS_CRYPT_ENCRYPTION_KEY_CREATION_UTIL_H_
