// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"

#include <pk11pub.h>
#include <secerr.h>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_util.h"
#include "crypto/aes_128_gcm_helpers_nss.h"
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

  // TODO(peter): For AES-GCM we should be using CKM_AES_GCM as the mechanism,
  // but because of an NSS bug we need to use CKM_AES_ECB as a work-around until
  // we require NSS 3.15+. https://bugzilla.mozilla.org/show_bug.cgi?id=853285
  // This does not affect the call to PK11{Decrypt,Encrypt}Helper below.
  const CK_MECHANISM_TYPE key_mechanism = CKM_AES_ECB;

  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());
  crypto::ScopedPK11SymKey aead_key(PK11_ImportSymKey(slot.get(), key_mechanism,
      PK11_OriginUnwrap, cka_mode, &key_item, nullptr));

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
    if (crypto::PK11EncryptHelper(aead_key.get(), CKM_AES_GCM, &param,
                                  raw_output, &output_length, output->size(),
                                  raw_input, input.size())
            != SECSuccess) {
      return false;
    }
  } else {
    if (crypto::PK11DecryptHelper(aead_key.get(), CKM_AES_GCM, &param,
                                  raw_output, &output_length, output->size(),
                                  raw_input, input.size())
            != SECSuccess) {
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
