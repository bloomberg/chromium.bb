// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/nss/sym_key_nss.h"

#include "base/logging.h"
#include "content/child/webcrypto/crypto_data.h"
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
                            blink::WebCryptoKeyUsageMask usage_mask,
                            unsigned keylen_bytes,
                            CK_MECHANISM_TYPE mechanism,
                            blink::WebCryptoKey* key) {
  DCHECK_NE(CKM_INVALID_MECHANISM, mechanism);

  crypto::ScopedPK11Slot slot(PK11_GetInternalKeySlot());
  if (!slot)
    return Status::OperationError();

  crypto::ScopedPK11SymKey pk11_key(
      PK11_KeyGen(slot.get(), mechanism, NULL, keylen_bytes, NULL));

  if (!pk11_key)
    return Status::OperationError();

  if (PK11_ExtractKeyValue(pk11_key.get()) != SECSuccess)
    return Status::OperationError();

  const SECItem* key_data = PK11_GetKeyData(pk11_key.get());
  if (!key_data)
    return Status::OperationError();

  scoped_ptr<SymKeyNss> handle(new SymKeyNss(
      pk11_key.Pass(), CryptoData(key_data->data, key_data->len)));

  *key = blink::WebCryptoKey::create(handle.release(),
                                     blink::WebCryptoKeyTypeSecret,
                                     extractable,
                                     algorithm,
                                     usage_mask);
  return Status::Success();
}

Status ImportKeyRawNss(const CryptoData& key_data,
                       const blink::WebCryptoKeyAlgorithm& algorithm,
                       bool extractable,
                       blink::WebCryptoKeyUsageMask usage_mask,
                       CK_MECHANISM_TYPE mechanism,
                       CK_FLAGS flags,
                       blink::WebCryptoKey* key) {
  DCHECK(!algorithm.isNull());
  SECItem key_item = MakeSECItemForBuffer(key_data);

  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());
  crypto::ScopedPK11SymKey pk11_sym_key(
      PK11_ImportSymKeyWithFlags(slot.get(),
                                 mechanism,
                                 PK11_OriginUnwrap,
                                 CKA_FLAGS_ONLY,
                                 &key_item,
                                 flags,
                                 false,
                                 NULL));
  if (!pk11_sym_key.get())
    return Status::OperationError();

  scoped_ptr<SymKeyNss> handle(new SymKeyNss(pk11_sym_key.Pass(), key_data));

  *key = blink::WebCryptoKey::create(handle.release(),
                                     blink::WebCryptoKeyTypeSecret,
                                     extractable,
                                     algorithm,
                                     usage_mask);
  return Status::Success();
}

}  // namespace webcrypto

}  // namespace content
