// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/nss/sym_key_nss.h"

#include "base/logging.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/generate_key_result.h"
#include "content/child/webcrypto/nss/key_nss.h"
#include "content/child/webcrypto/nss/util_nss.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/scoped_nss_types.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

Status GenerateSecretKeyNss(const blink::WebCryptoKeyAlgorithm& algorithm,
                            bool extractable,
                            blink::WebCryptoKeyUsageMask usages,
                            unsigned int keylen_bits,
                            CK_MECHANISM_TYPE mechanism,
                            GenerateKeyResult* result) {
  DCHECK_NE(CKM_INVALID_MECHANISM, mechanism);

  crypto::ScopedPK11Slot slot(PK11_GetInternalKeySlot());
  if (!slot)
    return Status::OperationError();

  std::vector<uint8_t> bytes(NumBitsToBytes(keylen_bits));
  if (bytes.size() > 0) {
    if (PK11_GenerateRandom(&bytes[0], bytes.size()) != SECSuccess)
      return Status::OperationError();
    TruncateToBitLength(keylen_bits, &bytes);
  }

  blink::WebCryptoKey key;
  Status status = ImportKeyRawNss(CryptoData(bytes), algorithm, extractable,
                                  usages, mechanism, &key);
  if (status.IsError())
    return status;

  result->AssignSecretKey(key);
  return Status::Success();
}

Status ImportKeyRawNss(const CryptoData& key_data,
                       const blink::WebCryptoKeyAlgorithm& algorithm,
                       bool extractable,
                       blink::WebCryptoKeyUsageMask usages,
                       CK_MECHANISM_TYPE mechanism,
                       blink::WebCryptoKey* key) {
  // The usages are enforced at the WebCrypto layer, so it isn't necessary to
  // create keys with limited usages.
  CK_FLAGS flags = kAllOperationFlags;

  DCHECK(!algorithm.isNull());
  SECItem key_item = MakeSECItemForBuffer(key_data);

  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());
  crypto::ScopedPK11SymKey pk11_sym_key(PK11_ImportSymKeyWithFlags(
      slot.get(), mechanism, PK11_OriginUnwrap, CKA_FLAGS_ONLY, &key_item,
      flags, false, NULL));
  if (!pk11_sym_key.get())
    return Status::OperationError();

  scoped_ptr<SymKeyNss> handle(new SymKeyNss(pk11_sym_key.Pass(), key_data));

  *key = blink::WebCryptoKey::create(handle.release(),
                                     blink::WebCryptoKeyTypeSecret, extractable,
                                     algorithm, usages);
  return Status::Success();
}

}  // namespace webcrypto

}  // namespace content
