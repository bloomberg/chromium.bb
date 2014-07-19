// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/nss/util_nss.h"

#include "base/lazy_instance.h"
#include "content/child/webcrypto/crypto_data.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"

#if defined(USE_NSS)
#include <dlfcn.h>
#include <secoid.h>
#endif

namespace content {

namespace webcrypto {

namespace {
base::LazyInstance<NssRuntimeSupport>::Leaky g_nss_runtime_support =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

// Creates a SECItem for the data in |buffer|. This does NOT make a copy, so
// |buffer| should outlive the SECItem.
SECItem MakeSECItemForBuffer(const CryptoData& buffer) {
  SECItem item = {
      siBuffer,
      // NSS requires non-const data even though it is just for input.
      const_cast<unsigned char*>(buffer.bytes()), buffer.byte_length()};
  return item;
}

CryptoData SECItemToCryptoData(const SECItem& item) {
  return CryptoData(item.data, item.len);
}

NssRuntimeSupport* NssRuntimeSupport::Get() {
  return &g_nss_runtime_support.Get();
}

NssRuntimeSupport::NssRuntimeSupport() : internal_slot_does_oaep_(false) {
#if !defined(USE_NSS)
  // Using a bundled version of NSS that is guaranteed to have this symbol.
  pk11_encrypt_func_ = PK11_Encrypt;
  pk11_decrypt_func_ = PK11_Decrypt;
  pk11_pub_encrypt_func_ = PK11_PubEncrypt;
  pk11_priv_decrypt_func_ = PK11_PrivDecrypt;
  internal_slot_does_oaep_ = true;
#else
  // Using system NSS libraries and PCKS #11 modules, which may not have the
  // necessary function (PK11_Encrypt) or mechanism support (CKM_AES_GCM).

  // If PK11_Encrypt() was successfully resolved, then NSS will support
  // AES-GCM directly. This was introduced in NSS 3.15.
  pk11_encrypt_func_ = reinterpret_cast<PK11_EncryptDecryptFunction>(
      dlsym(RTLD_DEFAULT, "PK11_Encrypt"));
  pk11_decrypt_func_ = reinterpret_cast<PK11_EncryptDecryptFunction>(
      dlsym(RTLD_DEFAULT, "PK11_Decrypt"));

  // Even though NSS's pk11wrap layer may support
  // PK11_PubEncrypt/PK11_PubDecrypt (introduced in NSS 3.16.2), it may have
  // loaded a softoken that does not include OAEP support.
  pk11_pub_encrypt_func_ = reinterpret_cast<PK11_PubEncryptFunction>(
      dlsym(RTLD_DEFAULT, "PK11_PubEncrypt"));
  pk11_priv_decrypt_func_ = reinterpret_cast<PK11_PrivDecryptFunction>(
      dlsym(RTLD_DEFAULT, "PK11_PrivDecrypt"));
  if (pk11_priv_decrypt_func_ && pk11_pub_encrypt_func_) {
    crypto::ScopedPK11Slot slot(PK11_GetInternalKeySlot());
    internal_slot_does_oaep_ =
        !!PK11_DoesMechanism(slot.get(), CKM_RSA_PKCS_OAEP);
  }
#endif
}

void PlatformInit() {
  crypto::EnsureNSSInit();
}

}  // namespace webcrypto

}  // namespace content
