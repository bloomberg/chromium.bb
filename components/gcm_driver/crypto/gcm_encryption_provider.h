// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_PROVIDER_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_PROVIDER_H_

#include <stdint.h>

#include <memory>
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
  // Result of decrypting an incoming message. The values of these reasons must
  // not be changed, because they are being recorded using UMA.
  enum DecryptionResult {
    // The message had not been encrypted by the sender.
    DECRYPTION_RESULT_UNENCRYPTED = 0,

    // The message had been encrypted by the sender, and could successfully be
    // decrypted for the registration it has been received for.
    DECRYPTION_RESULT_DECRYPTED = 1,

    // The contents of the Encryption HTTP header could not be parsed.
    DECRYPTION_RESULT_INVALID_ENCRYPTION_HEADER = 2,

    // The contents of the Crypto-Key HTTP header could not be parsed.
    DECRYPTION_RESULT_INVALID_CRYPTO_KEY_HEADER = 3,

    // No public/private key-pair was associated with the app_id.
    DECRYPTION_RESULT_NO_KEYS = 4,

    // The shared secret cannot be derived from the keying material.
    DECRYPTION_RESULT_INVALID_SHARED_SECRET = 5,

    // The payload could not be decrypted as AES-128-GCM.
    DECRYPTION_RESULT_INVALID_PAYLOAD = 6,

    DECRYPTION_RESULT_LAST = DECRYPTION_RESULT_INVALID_PAYLOAD
  };

  // Callback to be invoked when the public key and auth secret are available.
  using EncryptionInfoCallback = base::Callback<void(const std::string&,
                                                     const std::string&)>;

  // Callback to be invoked when a message may have been decrypted, as indicated
  // by the |result|. The |message| contains the dispatchable message in success
  // cases, or will be initialized to an empty, default state for failure.
  using MessageCallback = base::Callback<void(DecryptionResult result,
                                              const IncomingMessage& message)>;

  // Converts |result| to a string describing the details of said result.
  static std::string ToDecryptionResultDetailsString(DecryptionResult result);

  GCMEncryptionProvider();
  ~GCMEncryptionProvider();

  // Initializes the encryption provider with the |store_path| and the
  // |blocking_task_runner|. Done separately from the constructor in order to
  // avoid needing a blocking task runner for anything using GCMDriver.
  void Init(
      const base::FilePath& store_path,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);

  // Retrieves the public key and authentication secret associated with the
  // |app_id| + |authorized_entity| pair. Will create this info if necessary.
  // |authorized_entity| should be the InstanceID token's authorized entity, or
  // "" for non-InstanceID GCM registrations.
  void GetEncryptionInfo(const std::string& app_id,
                         const std::string& authorized_entity,
                         const EncryptionInfoCallback& callback);

  // Removes all encryption information associated with the |app_id| +
  // |authorized_entity| pair, then invokes |callback|. |authorized_entity|
  // should be the InstanceID token's authorized entity, or "*" to remove for
  // all InstanceID tokens, or "" for non-InstanceID GCM registrations.
  void RemoveEncryptionInfo(const std::string& app_id,
                            const std::string& authorized_entity,
                            const base::Closure& callback);

  // Determines whether |message| contains encrypted content.
  bool IsEncryptedMessage(const IncomingMessage& message) const;

  // Attempts to decrypt the |message|. If the |message| is not encrypted, the
  // |callback| will be invoked immediately. Otherwise |callback| will be called
  // asynchronously when |message| has been decrypted. A dispatchable message
  // will be used in case of success, an empty message in case of failure.
  void DecryptMessage(const std::string& app_id,
                      const IncomingMessage& message,
                      const MessageCallback& callback);

 private:
  friend class GCMEncryptionProviderTest;
  FRIEND_TEST_ALL_PREFIXES(GCMEncryptionProviderTest,
                           EncryptionRoundTripGCMRegistration);
  FRIEND_TEST_ALL_PREFIXES(GCMEncryptionProviderTest,
                           EncryptionRoundTripInstanceIDToken);

  void DidGetEncryptionInfo(const std::string& app_id,
                            const std::string& authorized_entity,
                            const EncryptionInfoCallback& callback,
                            const KeyPair& pair,
                            const std::string& auth_secret);

  void DidCreateEncryptionInfo(const EncryptionInfoCallback& callback,
                               const KeyPair& pair,
                               const std::string& auth_secret);

  void DecryptMessageWithKey(const IncomingMessage& message,
                             const MessageCallback& callback,
                             const std::string& salt,
                             const std::string& dh,
                             uint64_t rs,
                             const KeyPair& pair,
                             const std::string& auth_secret);

  std::unique_ptr<GCMKeyStore> key_store_;

  base::WeakPtrFactory<GCMEncryptionProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMEncryptionProvider);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_PROVIDER_H_
