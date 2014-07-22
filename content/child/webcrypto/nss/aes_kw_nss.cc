// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <secerr.h>

#include "base/numerics/safe_math.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/nss/aes_key_nss.h"
#include "content/child/webcrypto/nss/key_nss.h"
#include "content/child/webcrypto/nss/sym_key_nss.h"
#include "content/child/webcrypto/nss/util_nss.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/scoped_nss_types.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

// The Default IV for AES-KW. See http://www.ietf.org/rfc/rfc3394.txt
// Section 2.2.3.1.
const unsigned char kAesIv[] = {0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6};

// The result of unwrapping is a SymKey rather than a buffer. This is a
// consequence of how NSS exposes AES-KW. Subsequent code can extract the value
// of the sym key to interpret it as key bytes in another format.
Status DoUnwrapSymKeyAesKw(const CryptoData& wrapped_key_data,
                           PK11SymKey* wrapping_key,
                           CK_MECHANISM_TYPE mechanism,
                           CK_FLAGS flags,
                           crypto::ScopedPK11SymKey* unwrapped_key) {
  DCHECK_GE(wrapped_key_data.byte_length(), 24u);
  DCHECK_EQ(wrapped_key_data.byte_length() % 8, 0u);

  SECItem iv_item = MakeSECItemForBuffer(CryptoData(kAesIv, sizeof(kAesIv)));
  crypto::ScopedSECItem param_item(
      PK11_ParamFromIV(CKM_NSS_AES_KEY_WRAP, &iv_item));
  if (!param_item)
    return Status::ErrorUnexpected();

  SECItem cipher_text = MakeSECItemForBuffer(wrapped_key_data);

  // The plaintext length is always 64 bits less than the data size.
  const unsigned int plaintext_length = wrapped_key_data.byte_length() - 8;

#if defined(USE_NSS)
  // Part of workaround for
  // https://bugzilla.mozilla.org/show_bug.cgi?id=981170. See the explanation
  // later in this function.
  PORT_SetError(0);
#endif

  crypto::ScopedPK11SymKey new_key(
      PK11_UnwrapSymKeyWithFlags(wrapping_key,
                                 CKM_NSS_AES_KEY_WRAP,
                                 param_item.get(),
                                 &cipher_text,
                                 mechanism,
                                 CKA_FLAGS_ONLY,
                                 plaintext_length,
                                 flags));

  // TODO(padolph): Use NSS PORT_GetError() and friends to report a more
  // accurate error, providing if doesn't leak any information to web pages
  // about other web crypto users, key details, etc.
  if (!new_key)
    return Status::OperationError();

#if defined(USE_NSS)
  // Workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=981170
  // which was fixed in NSS 3.16.0.
  // If unwrap fails, NSS nevertheless returns a valid-looking PK11SymKey,
  // with a reasonable length but with key data pointing to uninitialized
  // memory.
  // To understand this workaround see the fix for 981170:
  // https://hg.mozilla.org/projects/nss/rev/753bb69e543c
  if (!NSS_VersionCheck("3.16") && PORT_GetError() == SEC_ERROR_BAD_DATA)
    return Status::OperationError();
#endif

  *unwrapped_key = new_key.Pass();
  return Status::Success();
}

Status WrapSymKeyAesKw(PK11SymKey* key,
                       PK11SymKey* wrapping_key,
                       std::vector<uint8_t>* buffer) {
  // The data size must be at least 16 bytes and a multiple of 8 bytes.
  // RFC 3394 does not specify a maximum allowed data length, but since only
  // keys are being wrapped in this application (which are small), a reasonable
  // max limit is whatever will fit into an unsigned. For the max size test,
  // note that AES Key Wrap always adds 8 bytes to the input data size.
  const unsigned int input_length = PK11_GetKeyLength(key);
  DCHECK_GE(input_length, 16u);
  DCHECK((input_length % 8) == 0);

  SECItem iv_item = MakeSECItemForBuffer(CryptoData(kAesIv, sizeof(kAesIv)));
  crypto::ScopedSECItem param_item(
      PK11_ParamFromIV(CKM_NSS_AES_KEY_WRAP, &iv_item));
  if (!param_item)
    return Status::ErrorUnexpected();

  base::CheckedNumeric<unsigned int> output_length = input_length;
  output_length += 8;
  if (!output_length.IsValid())
    return Status::ErrorDataTooLarge();

  buffer->resize(output_length.ValueOrDie());
  SECItem wrapped_key_item = MakeSECItemForBuffer(CryptoData(*buffer));

  if (SECSuccess != PK11_WrapSymKey(CKM_NSS_AES_KEY_WRAP,
                                    param_item.get(),
                                    wrapping_key,
                                    key,
                                    &wrapped_key_item)) {
    return Status::OperationError();
  }
  if (output_length.ValueOrDie() != wrapped_key_item.len)
    return Status::ErrorUnexpected();

  return Status::Success();
}

class AesKwCryptoAlgorithmNss : public AesAlgorithm {
 public:
  AesKwCryptoAlgorithmNss()
      : AesAlgorithm(
            CKM_NSS_AES_KEY_WRAP,
            CKF_WRAP | CKF_WRAP,
            blink::WebCryptoKeyUsageWrapKey | blink::WebCryptoKeyUsageUnwrapKey,
            "KW") {}

  virtual Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& wrapping_key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    if (data.byte_length() < 16)
      return Status::ErrorDataTooSmall();
    if (data.byte_length() % 8)
      return Status::ErrorInvalidAesKwDataLength();

    // Due to limitations in the NSS API for the AES-KW algorithm, |data| must
    // be temporarily viewed as a symmetric key to be wrapped (encrypted).
    SECItem data_item = MakeSECItemForBuffer(data);
    crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());
    crypto::ScopedPK11SymKey data_as_sym_key(
        PK11_ImportSymKey(slot.get(),
                          CKK_GENERIC_SECRET,
                          PK11_OriginUnwrap,
                          CKA_SIGN,
                          &data_item,
                          NULL));
    if (!data_as_sym_key)
      return Status::OperationError();

    return WrapSymKeyAesKw(
        data_as_sym_key.get(), SymKeyNss::Cast(wrapping_key)->key(), buffer);
  }

  virtual Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& wrapping_key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    if (data.byte_length() < 24)
      return Status::ErrorDataTooSmall();
    if (data.byte_length() % 8)
      return Status::ErrorInvalidAesKwDataLength();

    // Due to limitations in the NSS API for the AES-KW algorithm, |data| must
    // be temporarily viewed as a symmetric key to be unwrapped (decrypted).
    crypto::ScopedPK11SymKey decrypted;
    Status status = DoUnwrapSymKeyAesKw(data,
                                        SymKeyNss::Cast(wrapping_key)->key(),
                                        CKK_GENERIC_SECRET,
                                        0,
                                        &decrypted);
    if (status.IsError())
      return status;

    // Once the decrypt is complete, extract the resultant raw bytes from NSS
    // and return them to the caller.
    if (PK11_ExtractKeyValue(decrypted.get()) != SECSuccess)
      return Status::OperationError();
    const SECItem* const key_data = PK11_GetKeyData(decrypted.get());
    if (!key_data)
      return Status::OperationError();
    buffer->assign(key_data->data, key_data->data + key_data->len);

    return Status::Success();
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformAesKwImplementation() {
  return new AesKwCryptoAlgorithmNss;
}

}  // namespace webcrypto

}  // namespace content
