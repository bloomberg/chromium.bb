// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_PROVIDER_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_PROVIDER_H_

#include <stdint.h>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace gcm {

class GCMKeyStore;
class KeyPair;

// Provider that enables the GCM Driver to deal with encryption key management
// and decryption of incoming messages.
class GCMEncryptionProvider {
 public:
  // Callback to be invoked when the public encryption key is available.
  using PublicKeyCallback = base::Callback<void(const std::string&)>;

  GCMEncryptionProvider();
  ~GCMEncryptionProvider();

  // Initializes the encryption provider with the |store_path| and the
  // |blocking_task_runner|. Done separately from the constructor in order to
  // avoid needing a blocking task runner for anything using GCMDriver.
  void Init(
      const base::FilePath& store_path,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);

  // Retrieves the public encryption key belonging to |app_id|. If no keys have
  // been associated with |app_id| yet, they will be created.
  void GetPublicKey(const std::string& app_id,
                    const PublicKeyCallback& callback);

 private:
  void DidGetPublicKey(const std::string& app_id,
                       const PublicKeyCallback& callback,
                       const KeyPair& pair);

  void DidCreatePublicKey(const PublicKeyCallback& callback,
                          const KeyPair& pair);

  scoped_refptr<GCMKeyStore> key_store_;

  base::WeakPtrFactory<GCMEncryptionProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMEncryptionProvider);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_PROVIDER_H_
