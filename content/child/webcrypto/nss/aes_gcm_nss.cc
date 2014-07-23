// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/numerics/safe_math.h"
#include "base/stl_util.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/nss/aes_key_nss.h"
#include "content/child/webcrypto/nss/key_nss.h"
#include "content/child/webcrypto/nss/util_nss.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

// At the time of this writing:
//   * Windows and Mac builds ship with their own copy of NSS (3.15+)
//   * Linux builds use the system's libnss, which is 3.14 on Debian (but 3.15+
//     on other distros).
//
// Since NSS provides AES-GCM support starting in version 3.15, it may be
// unavailable for Linux Chrome users.
//
//  * !defined(CKM_AES_GCM)
//
//      This means that at build time, the NSS header pkcs11t.h is older than
//      3.15. However at runtime support may be present.
//
// TODO(eroman): Simplify this once 3.15+ is required by Linux builds.
#if !defined(CKM_AES_GCM)
#define CKM_AES_GCM 0x00001087

struct CK_GCM_PARAMS {
  CK_BYTE_PTR pIv;
  CK_ULONG ulIvLen;
  CK_BYTE_PTR pAAD;
  CK_ULONG ulAADLen;
  CK_ULONG ulTagBits;
};
#endif  // !defined(CKM_AES_GCM)

namespace content {

namespace webcrypto {

namespace {

Status NssSupportsAesGcm() {
  if (NssRuntimeSupport::Get()->IsAesGcmSupported())
    return Status::Success();
  return Status::ErrorUnsupported(
      "NSS version doesn't support AES-GCM. Try using version 3.15 or later");
}

// Helper to either encrypt or decrypt for AES-GCM. The result of encryption is
// the concatenation of the ciphertext and the authentication tag. Similarly,
// this is the expectation for the input to decryption.
Status AesGcmEncryptDecrypt(EncryptOrDecrypt mode,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const CryptoData& data,
                            std::vector<uint8_t>* buffer) {
  Status status = NssSupportsAesGcm();
  if (status.IsError())
    return status;

  PK11SymKey* sym_key = SymKeyNss::Cast(key)->key();
  const blink::WebCryptoAesGcmParams* params = algorithm.aesGcmParams();
  if (!params)
    return Status::ErrorUnexpected();

  unsigned int tag_length_bits;
  status = GetAesGcmTagLengthInBits(params, &tag_length_bits);
  if (status.IsError())
    return status;
  unsigned int tag_length_bytes = tag_length_bits / 8;

  CryptoData iv(params->iv());
  CryptoData additional_data(params->optionalAdditionalData());

  CK_GCM_PARAMS gcm_params = {0};
  gcm_params.pIv = const_cast<unsigned char*>(iv.bytes());
  gcm_params.ulIvLen = iv.byte_length();

  gcm_params.pAAD = const_cast<unsigned char*>(additional_data.bytes());
  gcm_params.ulAADLen = additional_data.byte_length();

  gcm_params.ulTagBits = tag_length_bits;

  SECItem param;
  param.type = siBuffer;
  param.data = reinterpret_cast<unsigned char*>(&gcm_params);
  param.len = sizeof(gcm_params);

  base::CheckedNumeric<unsigned int> buffer_size(data.byte_length());

  // Calculate the output buffer size.
  if (mode == ENCRYPT) {
    buffer_size += tag_length_bytes;
    if (!buffer_size.IsValid())
      return Status::ErrorDataTooLarge();
  }

  // TODO(eroman): In theory the buffer allocated for the plain text should be
  // sized as |data.byte_length() - tag_length_bytes|.
  //
  // However NSS has a bug whereby it will fail if the output buffer size is
  // not at least as large as the ciphertext:
  //
  // https://bugzilla.mozilla.org/show_bug.cgi?id=%20853674
  //
  // From the analysis of that bug it looks like it might be safe to pass a
  // correctly sized buffer but lie about its size. Since resizing the
  // WebCryptoArrayBuffer is expensive that hack may be worth looking into.

  buffer->resize(buffer_size.ValueOrDie());
  unsigned char* buffer_data = vector_as_array(buffer);

  PK11_EncryptDecryptFunction encrypt_or_decrypt_func =
      (mode == ENCRYPT) ? NssRuntimeSupport::Get()->pk11_encrypt_func()
                        : NssRuntimeSupport::Get()->pk11_decrypt_func();

  unsigned int output_len = 0;
  SECStatus result = encrypt_or_decrypt_func(sym_key,
                                             CKM_AES_GCM,
                                             &param,
                                             buffer_data,
                                             &output_len,
                                             buffer->size(),
                                             data.bytes(),
                                             data.byte_length());

  if (result != SECSuccess)
    return Status::OperationError();

  // Unfortunately the buffer needs to be shrunk for decryption (see the NSS bug
  // above).
  buffer->resize(output_len);

  return Status::Success();
}

class AesGcmImplementation : public AesAlgorithm {
 public:
  AesGcmImplementation() : AesAlgorithm(CKM_AES_GCM, "GCM") {}

  virtual Status VerifyKeyUsagesBeforeImportKey(
      blink::WebCryptoKeyFormat format,
      blink::WebCryptoKeyUsageMask usage_mask) const OVERRIDE {
    // Prevent importing AES-GCM keys if it is unavailable.
    Status status = NssSupportsAesGcm();
    if (status.IsError())
      return status;
    return AesAlgorithm::VerifyKeyUsagesBeforeImportKey(format, usage_mask);
  }

  virtual Status VerifyKeyUsagesBeforeGenerateKey(
      blink::WebCryptoKeyUsageMask usage_mask) const OVERRIDE {
    // Prevent generating AES-GCM keys if it is unavailable.
    Status status = NssSupportsAesGcm();
    if (status.IsError())
      return status;
    return AesAlgorithm::VerifyKeyUsagesBeforeGenerateKey(usage_mask);
  }

  virtual Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    return AesGcmEncryptDecrypt(ENCRYPT, algorithm, key, data, buffer);
  }

  virtual Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    return AesGcmEncryptDecrypt(DECRYPT, algorithm, key, data, buffer);
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformAesGcmImplementation() {
  return new AesGcmImplementation;
}

}  // namespace webcrypto

}  // namespace content
