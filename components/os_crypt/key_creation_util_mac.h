// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_KEY_CREATION_UTIL_MAC_H_
#define COMPONENTS_OS_CRYPT_KEY_CREATION_UTIL_MAC_H_

#include "base/memory/scoped_refptr.h"

class PrefService;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace os_crypt {

// A utility class which provides a method to check whether the encryption key
// should be available in the Keychain (meaning it was created in the past).
class KeyCreationUtilMac {
 public:
  // This class has to be initialized on the main UI thread since it uses
  // the local state.
  KeyCreationUtilMac(
      PrefService* local_state,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  ~KeyCreationUtilMac();

  // This method doesn't need to be called on the main thread.
  bool key_already_created() { return key_already_created_; }

  // This asynchronously updates the preference on the main thread that the key
  // was created. This method is called when key is added to the Keychain, or
  // the first time the key is successfully retrieved from the Keychain and the
  // preference hasn't been set yet. This method doesn't need to be called on
  // the main thread.
  void OnKeyWasStored();

 private:
  PrefService* local_state_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  volatile bool key_already_created_;
};

}  // namespace os_crypt

#endif  // COMPONENTS_OS_CRYPT_KEY_CREATION_UTIL_MAC_H_
