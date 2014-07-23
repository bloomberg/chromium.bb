// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cryptohi.h>

#include "base/numerics/safe_math.h"
#include "base/stl_util.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/nss/aes_key_nss.h"
#include "content/child/webcrypto/nss/key_nss.h"
#include "content/child/webcrypto/nss/util_nss.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/scoped_nss_types.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace content {

namespace webcrypto {

namespace {

Status AesCbcEncryptDecrypt(EncryptOrDecrypt mode,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const CryptoData& data,
                            std::vector<uint8_t>* buffer) {
  const blink::WebCryptoAesCbcParams* params = algorithm.aesCbcParams();
  if (!params)
    return Status::ErrorUnexpected();

  CryptoData iv(params->iv().data(), params->iv().size());
  if (iv.byte_length() != 16)
    return Status::ErrorIncorrectSizeAesCbcIv();

  PK11SymKey* sym_key = SymKeyNss::Cast(key)->key();

  CK_ATTRIBUTE_TYPE operation = (mode == ENCRYPT) ? CKA_ENCRYPT : CKA_DECRYPT;

  SECItem iv_item = MakeSECItemForBuffer(iv);

  crypto::ScopedSECItem param(PK11_ParamFromIV(CKM_AES_CBC_PAD, &iv_item));
  if (!param)
    return Status::OperationError();

  crypto::ScopedPK11Context context(PK11_CreateContextBySymKey(
      CKM_AES_CBC_PAD, operation, sym_key, param.get()));

  if (!context.get())
    return Status::OperationError();

  // Oddly PK11_CipherOp takes input and output lengths as "int" rather than
  // "unsigned int". Do some checks now to avoid integer overflowing.
  base::CheckedNumeric<int> output_max_len = data.byte_length();
  output_max_len += AES_BLOCK_SIZE;
  if (!output_max_len.IsValid()) {
    // TODO(eroman): Handle this by chunking the input fed into NSS. Right now
    // it doesn't make much difference since the one-shot API would end up
    // blowing out the memory and crashing anyway.
    return Status::ErrorDataTooLarge();
  }

  // PK11_CipherOp does an invalid memory access when given empty decryption
  // input, or input which is not a multiple of the block size. See also
  // https://bugzilla.mozilla.com/show_bug.cgi?id=921687.
  if (operation == CKA_DECRYPT &&
      (data.byte_length() == 0 || (data.byte_length() % AES_BLOCK_SIZE != 0))) {
    return Status::OperationError();
  }

  // TODO(eroman): Refine the output buffer size. It can be computed exactly for
  //               encryption, and can be smaller for decryption.
  buffer->resize(output_max_len.ValueOrDie());

  unsigned char* buffer_data = vector_as_array(buffer);

  int output_len;
  if (SECSuccess != PK11_CipherOp(context.get(),
                                  buffer_data,
                                  &output_len,
                                  buffer->size(),
                                  data.bytes(),
                                  data.byte_length())) {
    return Status::OperationError();
  }

  unsigned int final_output_chunk_len;
  if (SECSuccess !=
      PK11_DigestFinal(context.get(),
                       buffer_data + output_len,
                       &final_output_chunk_len,
                       (output_max_len - output_len).ValueOrDie())) {
    return Status::OperationError();
  }

  buffer->resize(final_output_chunk_len + output_len);
  return Status::Success();
}

class AesCbcImplementation : public AesAlgorithm {
 public:
  AesCbcImplementation() : AesAlgorithm(CKM_AES_CBC, "CBC") {}

  virtual Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    return AesCbcEncryptDecrypt(ENCRYPT, algorithm, key, data, buffer);
  }

  virtual Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    return AesCbcEncryptDecrypt(DECRYPT, algorithm, key, data, buffer);
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformAesCbcImplementation() {
  return new AesCbcImplementation;
}

}  // namespace webcrypto

}  // namespace content
