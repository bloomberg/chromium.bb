// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_GCM_DECRYPTION_RESULT_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_GCM_DECRYPTION_RESULT_H_

#include <string>

namespace gcm {

// Result of decrypting an incoming message. The values of these reasons must
// not be changed as they are being recorded using UMA. When adding a value,
// please update GCMDecryptionResult in //tools/metrics/histograms/enums.xml.
enum class GCMDecryptionResult {
  // The message had not been encrypted by the sender.
  UNENCRYPTED = 0,

  // The message had been encrypted by the sender, and could successfully be
  // decrypted for the registration it has been received for. The encryption
  // scheme used for the message was draft-ietf-webpush-encryption-03.
  DECRYPTED_DRAFT_03 = 1,

  // The contents of the Encryption HTTP header could not be parsed.
  INVALID_ENCRYPTION_HEADER = 2,

  // The contents of the Crypto-Key HTTP header could not be parsed.
  INVALID_CRYPTO_KEY_HEADER = 3,

  // No public/private key-pair was associated with the app_id.
  NO_KEYS = 4,

  // The shared secret cannot be derived from the keying material.
  INVALID_SHARED_SECRET = 5,

  // The payload could not be decrypted as AES-128-GCM.
  INVALID_PAYLOAD = 6,

  // The binary header leading the ciphertext could not be parsed. Only
  // applicable to messages encrypted per draft-ietf-webpush-encryption-08.
  INVALID_BINARY_HEADER = 7,

  // The message had been encrypted by the sender, and could successfully be
  // decrypted for the registration it has been received for. The encryption
  // scheme used for the message was draft-ietf-webpush-encryption-08.
  DECRYPTED_DRAFT_08 = 8,

  // Should be one more than the otherwise highest value in this enumeration.
  ENUM_SIZE = DECRYPTED_DRAFT_08 + 1
};

// Converts the GCMDecryptionResult value to a string that can be used to
// explain the issue on chrome://gcm-internals/.
std::string ToGCMDecryptionResultDetailsString(GCMDecryptionResult result);

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_GCM_DECRYPTION_RESULT_H_
