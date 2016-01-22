// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_PROVIDER_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_PROVIDER_H_

#include <stdint.h>
#include <string>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace gcm {

class GCMKeyStore;
struct IncomingMessage;
class KeyPair;

// Provider that enables the GCM Driver to deal with encryption key management
// and decryption of incoming messages.
class GCMEncryptionProvider {
 public:
  // Callback to be invoked when the public key and auth secret are available.
  using EncryptionInfoCallback = base::Callback<void(const std::string&,
                                                     const std::string&)>;

  // Callback to be invoked when a message has been decrypted.
  using MessageDecryptedCallback = base::Callback<void(const IncomingMessage&)>;

  // Reasons why the decryption of an incoming message can fail.
  enum DecryptionFailure {
    DECRYPTION_FAILURE_UNKNOWN,

    // The contents of the Encryption HTTP header could not be parsed.
    DECRYPTION_FAILURE_INVALID_ENCRYPTION_HEADER,

    // The contents of the Crypto-Key HTTP header could not be parsed.
    DECRYPTION_FAILURE_INVALID_CRYPTO_KEY_HEADER,

    // No public/private key-pair was associated with the app_id.
    DECRYPTION_FAILURE_NO_KEYS,

    // The public key provided in the Crypto-Key header is invalid.
    DECRYPTION_FAILURE_INVALID_PUBLIC_KEY,

    // The payload could not be decrypted as AES-128-GCM.
    DECRYPTION_FAILURE_INVALID_PAYLOAD
  };

  // Callback to be invoked when a message cannot be decoded.
  using DecryptionFailedCallback = base::Callback<void(DecryptionFailure)>;

  // Converts |reason| to a string describing the details of said reason.
  static std::string ToDecryptionFailureDetailsString(DecryptionFailure reason);

  GCMEncryptionProvider();
  ~GCMEncryptionProvider();

  // Initializes the encryption provider with the |store_path| and the
  // |blocking_task_runner|. Done separately from the constructor in order to
  // avoid needing a blocking task runner for anything using GCMDriver.
  void Init(
      const base::FilePath& store_path,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);

  // Retrieves the public key and authentication secret associated with the
  // |app_id|. If none have been associated yet, they will be created.
  void GetEncryptionInfo(const std::string& app_id,
                         const EncryptionInfoCallback& callback);

  // Determines whether |message| contains encrypted content.
  bool IsEncryptedMessage(const IncomingMessage& message) const;

  // Asynchronously decrypts |message|. The |success_callback| will be invoked
  // the message could be decrypted successfully, accompanied by the decrypted
  // payload of the message. When decryption failed, the |failure_callback| will
  // be invoked with the reason that encryption failed.
  void DecryptMessage(const std::string& app_id,
                      const IncomingMessage& message,
                      const MessageDecryptedCallback& success_callback,
                      const DecryptionFailedCallback& failure_callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(GCMEncryptionProviderTest, EncryptionRoundTrip);

  void DidGetEncryptionInfo(const std::string& app_id,
                            const EncryptionInfoCallback& callback,
                            const KeyPair& pair,
                            const std::string& auth_secret);

  void DidCreateEncryptionInfo(const EncryptionInfoCallback& callback,
                               const KeyPair& pair,
                               const std::string& auth_secret);

  void DecryptMessageWithKey(const IncomingMessage& message,
                             const MessageDecryptedCallback& success_callback,
                             const DecryptionFailedCallback& failure_callback,
                             const std::string& salt,
                             const std::string& dh,
                             uint64_t rs,
                             const KeyPair& pair,
                             const std::string& auth_secret);

  scoped_ptr<GCMKeyStore> key_store_;

  base::WeakPtrFactory<GCMEncryptionProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMEncryptionProvider);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_PROVIDER_H_
