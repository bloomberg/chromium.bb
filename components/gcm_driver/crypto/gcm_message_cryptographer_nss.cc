// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"

#include <pk11pub.h>
#include <secerr.h>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_util.h"
#include "crypto/scoped_nss_types.h"

namespace gcm {

bool GCMMessageCryptographer::EncryptDecryptRecordInternal(
    Mode mode,
    const base::StringPiece& input,
    const base::StringPiece& key,
    const base::StringPiece& nonce,
    std::string* output) const {
  DCHECK(output);

  SECItem key_item;
  key_item.type = siBuffer;
  key_item.data = const_cast<unsigned char*>(
      reinterpret_cast<const unsigned char*>(key.data()));
  key_item.len = key.size();

  const CK_ATTRIBUTE_TYPE cka_mode = mode == ENCRYPT ? CKA_ENCRYPT
                                                     : CKA_DECRYPT;

  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());
  crypto::ScopedPK11SymKey aead_key(
      PK11_ImportSymKey(slot.get(), CKM_AES_GCM, PK11_OriginUnwrap, cka_mode,
                        &key_item, nullptr));

  CK_GCM_PARAMS gcm_params;
  gcm_params.pIv = const_cast<unsigned char*>(
      reinterpret_cast<const unsigned char*>(nonce.data()));
  gcm_params.ulIvLen = nonce.size();

  gcm_params.pAAD = nullptr;
  gcm_params.ulAADLen = 0;

  gcm_params.ulTagBits = kAuthenticationTagBytes * 8;

  SECItem param;
  param.type = siBuffer;
  param.data = reinterpret_cast<unsigned char*>(&gcm_params);
  param.len = sizeof(gcm_params);

  base::CheckedNumeric<size_t> maximum_output_length(input.size());
  if (mode == ENCRYPT)
    maximum_output_length += kAuthenticationTagBytes;

  // WriteInto requires the buffer to finish with a NULL-byte.
  maximum_output_length += 1;

  unsigned int output_length = 0;
  unsigned char* raw_input = const_cast<unsigned char*>(
      reinterpret_cast<const unsigned char*>(input.data()));
  unsigned char* raw_output = reinterpret_cast<unsigned char*>(
      base::WriteInto(output, maximum_output_length.ValueOrDie()));

  if (mode == ENCRYPT) {
    if (PK11_Encrypt(aead_key.get(), CKM_AES_GCM, &param, raw_output,
                     &output_length, output->size(), raw_input,
                     input.size()) != SECSuccess) {
      return false;
    }
  } else {
    if (PK11_Decrypt(aead_key.get(), CKM_AES_GCM, &param, raw_output,
                     &output_length, output->size(), raw_input,
                     input.size()) != SECSuccess) {
      return false;
    }
  }

  base::CheckedNumeric<size_t> expected_output_length(input.size());
  if (mode == ENCRYPT)
    expected_output_length += kAuthenticationTagBytes;
  else
    expected_output_length -= kAuthenticationTagBytes;

  DCHECK_EQ(expected_output_length.ValueOrDie(), output_length);

  output->resize(output_length);
  return true;
}

}  // namespace gcm
