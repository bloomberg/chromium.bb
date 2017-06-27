// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_decryption_result.h"

#include "base/logging.h"

namespace gcm {

std::string ToGCMDecryptionResultDetailsString(GCMDecryptionResult result) {
  switch (result) {
    case GCMDecryptionResult::UNENCRYPTED:
      return "Message was not encrypted";
    case GCMDecryptionResult::DECRYPTED_DRAFT_03:
      return "Message decrypted (draft 03)";
    case GCMDecryptionResult::DECRYPTED_DRAFT_08:
      return "Message decrypted (draft 08)";
    case GCMDecryptionResult::INVALID_ENCRYPTION_HEADER:
      return "Invalid format for the Encryption header";
    case GCMDecryptionResult::INVALID_CRYPTO_KEY_HEADER:
      return "Invalid format for the Crypto-Key header";
    case GCMDecryptionResult::NO_KEYS:
      return "There are no associated keys with the subscription";
    case GCMDecryptionResult::INVALID_SHARED_SECRET:
      return "The shared secret cannot be derived from the keying material";
    case GCMDecryptionResult::INVALID_PAYLOAD:
      return "AES-GCM decryption failed";
    case GCMDecryptionResult::INVALID_BINARY_HEADER:
      return "The message's binary header could not be parsed.";
    case GCMDecryptionResult::ENUM_SIZE:
      break;  // deliberate fall-through
  }

  NOTREACHED();
  return "(invalid result)";
}

}  // namespace gcm
