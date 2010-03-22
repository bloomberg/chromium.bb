// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/pbkdf2.h"

#include <nss.h>
#include <pk11pub.h>

#include "base/crypto/scoped_nss_types.h"
#include "base/nss_util.h"

namespace base {

SymmetricKey* DeriveKeyFromPassword(const std::string& password,
                                    const std::string& salt,
                                    unsigned int iterations,
                                    unsigned int key_size) {
  EnsureNSSInit();
  if (salt.empty() || iterations == 0 || key_size == 0)
    return NULL;

  SECItem password_item;
  password_item.type = siBuffer;
  password_item.data = reinterpret_cast<unsigned char*>(
      const_cast<char *>(password.data()));
  password_item.len = password.size();

  SECItem salt_item;
  salt_item.type = siBuffer;
  salt_item.data = reinterpret_cast<unsigned char*>(
      const_cast<char *>(salt.data()));
  salt_item.len = salt.size();

  ScopedSECAlgorithmID alg_id(PK11_CreatePBEV2AlgorithmID(SEC_OID_PKCS5_PBKDF2,
                                                          SEC_OID_PKCS5_PBKDF2,
                                                          SEC_OID_HMAC_SHA1,
                                                          key_size,
                                                          iterations,
                                                          &salt_item));
  if (!alg_id.get())
    return NULL;

  ScopedPK11Slot slot(PK11_GetBestSlot(SEC_OID_PKCS5_PBKDF2, NULL));
  if (!slot.get())
    return NULL;

  PK11SymKey* sym_key = PK11_PBEKeyGen(slot.get(), alg_id.get(), &password_item,
                                       PR_FALSE, NULL);
  if (!sym_key)
    return NULL;

  return new SymmetricKey(sym_key);
}

}  // namespace base
