// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/aes_128_gcm_decrypter.h"

#include <nss.h>
#include <pk11pub.h>
#include <secerr.h>

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/ghash.h"
#include "crypto/scoped_nss_types.h"

#if defined(USE_NSS)
#include <dlfcn.h>
#endif

using base::StringPiece;

namespace net {

namespace {

// The pkcs11t.h header in NSS versions older than 3.14 does not have the CTR
// and GCM types, so define them here.
#if !defined(CKM_AES_CTR)
#define CKM_AES_CTR 0x00001086
#define CKM_AES_GCM 0x00001087

struct CK_AES_CTR_PARAMS {
  CK_ULONG ulCounterBits;
  CK_BYTE cb[16];
};

struct CK_GCM_PARAMS {
  CK_BYTE_PTR pIv;
  CK_ULONG ulIvLen;
  CK_BYTE_PTR pAAD;
  CK_ULONG ulAADLen;
  CK_ULONG ulTagBits;
};
#endif  // CKM_AES_CTR

typedef SECStatus
(*PK11_DecryptFunction)(
    PK11SymKey* symKey, CK_MECHANISM_TYPE mechanism, SECItem* param,
    unsigned char* out, unsigned int* outLen, unsigned int maxLen,
    const unsigned char* enc, unsigned encLen);

// On Linux, dynamically link against the system version of libnss3.so. In
// order to continue working on systems without up-to-date versions of NSS,
// lookup PK11_Decrypt with dlsym.

// GcmSupportChecker is a singleton which caches the results of runtime symbol
// resolution of PK11_Decrypt.
class GcmSupportChecker {
 public:
  static PK11_DecryptFunction pk11_decrypt_func() {
    return pk11_decrypt_func_;
  }

  static CK_MECHANISM_TYPE aes_key_mechanism() {
    return aes_key_mechanism_;
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<GcmSupportChecker>;

  GcmSupportChecker() {
#if !defined(USE_NSS)
    // Using a bundled version of NSS that is guaranteed to have this symbol.
    pk11_decrypt_func_ = PK11_Decrypt;
#else
    // Using system NSS libraries and PCKS #11 modules, which may not have the
    // necessary function (PK11_Decrypt) or mechanism support (CKM_AES_GCM).

    // If PK11_Decrypt() was successfully resolved, then NSS will support
    // AES-GCM directly. This was introduced in NSS 3.15.
    pk11_decrypt_func_ = (PK11_DecryptFunction)dlsym(RTLD_DEFAULT,
                                                     "PK11_Decrypt");
    if (pk11_decrypt_func_ == NULL) {
      aes_key_mechanism_ = CKM_AES_ECB;
    }
#endif
  }

  // |pk11_decrypt_func_| stores the runtime symbol resolution of PK11_Decrypt.
  static PK11_DecryptFunction pk11_decrypt_func_;

  // The correct value for |aes_key_mechanism_| is CKM_AES_GCM, but because of
  // NSS bug https://bugzilla.mozilla.org/show_bug.cgi?id=853285 (to be fixed in
  // NSS 3.15), use CKM_AES_ECB for NSS versions older than 3.15.
  static CK_MECHANISM_TYPE aes_key_mechanism_;
};

// static
PK11_DecryptFunction GcmSupportChecker::pk11_decrypt_func_ = NULL;

// static
CK_MECHANISM_TYPE GcmSupportChecker::aes_key_mechanism_ = CKM_AES_GCM;

base::LazyInstance<GcmSupportChecker>::Leaky g_gcm_support_checker =
    LAZY_INSTANCE_INITIALIZER;

const size_t kKeySize = 16;
const size_t kNoncePrefixSize = 4;
const size_t kAuthTagSize = 16;

// Calls PK11_Decrypt if it's available.  Otherwise, emulates CKM_AES_GCM using
// CKM_AES_CTR and the GaloisHash class.
SECStatus My_Decrypt(PK11SymKey* key,
                     CK_MECHANISM_TYPE mechanism,
                     SECItem* param,
                     unsigned char* out,
                     unsigned int* out_len,
                     unsigned int max_len,
                     const unsigned char* enc,
                     unsigned int enc_len) {
  // If PK11_Decrypt() was successfully resolved or if bundled version of NSS is
  // being used, then NSS will support AES-GCM directly.
  PK11_DecryptFunction pk11_decrypt_func =
      GcmSupportChecker::pk11_decrypt_func();
  if (pk11_decrypt_func != NULL) {
    return pk11_decrypt_func(key, mechanism, param, out, out_len, max_len, enc,
                             enc_len);
  }

  // Otherwise, the user has an older version of NSS. Regrettably, NSS 3.14.x
  // has a bug in the AES GCM code
  // (https://bugzilla.mozilla.org/show_bug.cgi?id=853285), as well as missing
  // the PK11_Decrypt function
  // (https://bugzilla.mozilla.org/show_bug.cgi?id=854063), both of which are
  // resolved in NSS 3.15.

  DCHECK_EQ(mechanism, static_cast<CK_MECHANISM_TYPE>(CKM_AES_GCM));
  DCHECK_EQ(param->len, sizeof(CK_GCM_PARAMS));

  const CK_GCM_PARAMS* gcm_params =
      reinterpret_cast<CK_GCM_PARAMS*>(param->data);

  DCHECK_EQ(gcm_params->ulTagBits, kAuthTagSize * 8);
  if (gcm_params->ulIvLen != 12u) {
    DLOG(INFO) << "ulIvLen is not equal to 12";
    PORT_SetError(SEC_ERROR_INPUT_LEN);
    return SECFailure;
  }

  SECItem my_param = { siBuffer, NULL, 0 };

  // Step 2. Let H = CIPH_K(128 '0' bits).
  unsigned char ghash_key[16] = {0};
  crypto::ScopedPK11Context ctx(PK11_CreateContextBySymKey(
      CKM_AES_ECB, CKA_ENCRYPT, key, &my_param));
  if (!ctx) {
    DLOG(INFO) << "PK11_CreateContextBySymKey failed";
    return SECFailure;
  }
  int output_len;
  if (PK11_CipherOp(ctx.get(), ghash_key, &output_len, sizeof(ghash_key),
                    ghash_key, sizeof(ghash_key)) != SECSuccess) {
    DLOG(INFO) << "PK11_CipherOp failed";
    return SECFailure;
  }

  PK11_Finalize(ctx.get());

  if (output_len != sizeof(ghash_key)) {
    DLOG(INFO) << "Wrong output length";
    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    return SECFailure;
  }

  // Step 3. If len(IV)=96, then let J0 = IV || 31 '0' bits || 1.
  CK_AES_CTR_PARAMS ctr_params = {0};
  ctr_params.ulCounterBits = 32;
  memcpy(ctr_params.cb, gcm_params->pIv, gcm_params->ulIvLen);
  ctr_params.cb[12] = 0;
  ctr_params.cb[13] = 0;
  ctr_params.cb[14] = 0;
  ctr_params.cb[15] = 1;

  my_param.type = siBuffer;
  my_param.data = reinterpret_cast<unsigned char*>(&ctr_params);
  my_param.len = sizeof(ctr_params);

  ctx.reset(PK11_CreateContextBySymKey(CKM_AES_CTR, CKA_ENCRYPT, key,
                                       &my_param));
  if (!ctx) {
    DLOG(INFO) << "PK11_CreateContextBySymKey failed";
    return SECFailure;
  }

  // Step 6. Calculate the encryption mask of GCTR_K(J0, ...).
  unsigned char tag_mask[16] = {0};
  if (PK11_CipherOp(ctx.get(), tag_mask, &output_len, sizeof(tag_mask),
                    tag_mask, sizeof(tag_mask)) != SECSuccess) {
    DLOG(INFO) << "PK11_CipherOp failed";
    return SECFailure;
  }
  if (output_len != sizeof(tag_mask)) {
    DLOG(INFO) << "Wrong output length";
    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    return SECFailure;
  }

  if (enc_len < kAuthTagSize) {
    PORT_SetError(SEC_ERROR_INPUT_LEN);
    return SECFailure;
  }

  // The const_cast for |enc| can be removed if system NSS libraries are
  // NSS 3.14.1 or later (NSS bug
  // https://bugzilla.mozilla.org/show_bug.cgi?id=808218).
  if (PK11_CipherOp(ctx.get(), out, &output_len, max_len,
                    const_cast<unsigned char*>(enc),
                    enc_len - kAuthTagSize) != SECSuccess) {
    DLOG(INFO) << "PK11_CipherOp failed";
    return SECFailure;
  }

  PK11_Finalize(ctx.get());

  if (static_cast<unsigned int>(output_len) != enc_len - kAuthTagSize) {
    DLOG(INFO) << "Wrong output length";
    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    return SECFailure;
  }

  crypto::GaloisHash ghash(ghash_key);
  ghash.UpdateAdditional(gcm_params->pAAD, gcm_params->ulAADLen);
  ghash.UpdateCiphertext(enc, output_len);
  unsigned char auth_tag[kAuthTagSize];
  ghash.Finish(auth_tag, kAuthTagSize);
  for (unsigned int i = 0; i < kAuthTagSize; i++) {
    auth_tag[i] ^= tag_mask[i];
  }

  if (NSS_SecureMemcmp(auth_tag, enc + output_len, kAuthTagSize) != 0) {
    PORT_SetError(SEC_ERROR_BAD_DATA);
    return SECFailure;
  }

  *out_len = output_len;
  return SECSuccess;
}

}  // namespace

Aes128GcmDecrypter::Aes128GcmDecrypter() {
  ignore_result(g_gcm_support_checker.Get());
}

// static
bool Aes128GcmDecrypter::IsSupported() {
  // NSS 3.15 supports CKM_AES_GCM directly.
  // NSS 3.14 supports CKM_AES_CTR, which can be used to emulate CKM_AES_GCM.
  // Versions earlier than NSS 3.14 are not supported.
  return NSS_VersionCheck("3.14") != PR_FALSE;
}

bool Aes128GcmDecrypter::SetKey(StringPiece key) {
  DCHECK_EQ(key.size(), sizeof(key_));
  if (key.size() != sizeof(key_)) {
    return false;
  }
  memcpy(key_, key.data(), key.size());
  return true;
}

bool Aes128GcmDecrypter::SetNoncePrefix(StringPiece nonce_prefix) {
  DCHECK_EQ(nonce_prefix.size(), kNoncePrefixSize);
  if (nonce_prefix.size() != kNoncePrefixSize) {
    return false;
  }
  memcpy(nonce_, nonce_prefix.data(), nonce_prefix.size());
  return true;
}

bool Aes128GcmDecrypter::Decrypt(StringPiece nonce,
                                 StringPiece associated_data,
                                 StringPiece ciphertext,
                                 unsigned char* output,
                                 size_t* output_length) {
  if (ciphertext.length() < kAuthTagSize ||
      nonce.size() != kNoncePrefixSize + sizeof(QuicPacketSequenceNumber)) {
    return false;
  }
  // NSS 3.14.x incorrectly requires an output buffer at least as long as
  // the ciphertext (NSS bug
  // https://bugzilla.mozilla.org/show_bug.cgi?id= 853674). Fortunately
  // QuicDecrypter::Decrypt() specifies that |output| must be as long as
  // |ciphertext| on entry.
  size_t plaintext_size = ciphertext.length() - kAuthTagSize;

  // Import key_ into NSS.
  SECItem key_item;
  key_item.type = siBuffer;
  key_item.data = key_;
  key_item.len = sizeof(key_);
  PK11SlotInfo* slot = PK11_GetInternalSlot();
  // The exact value of the |origin| argument doesn't matter to NSS as long as
  // it's not PK11_OriginFortezzaHack, so pass PK11_OriginUnwrap as a
  // placeholder.
  crypto::ScopedPK11SymKey aes_key(PK11_ImportSymKey(
      slot, GcmSupportChecker::aes_key_mechanism(), PK11_OriginUnwrap,
      CKA_DECRYPT, &key_item, NULL));
  PK11_FreeSlot(slot);
  slot = NULL;
  if (!aes_key) {
    DLOG(INFO) << "PK11_ImportSymKey failed";
    return false;
  }

  CK_GCM_PARAMS gcm_params = {0};
  gcm_params.pIv =
      reinterpret_cast<CK_BYTE*>(const_cast<char*>(nonce.data()));
  gcm_params.ulIvLen = nonce.size();
  gcm_params.pAAD =
      reinterpret_cast<CK_BYTE*>(const_cast<char*>(associated_data.data()));
  gcm_params.ulAADLen = associated_data.size();
  gcm_params.ulTagBits = kAuthTagSize * 8;

  SECItem param;
  param.type = siBuffer;
  param.data = reinterpret_cast<unsigned char*>(&gcm_params);
  param.len = sizeof(gcm_params);

  unsigned int output_len;
  // If an incorrect authentication tag causes a decryption failure, the NSS
  // error is SEC_ERROR_BAD_DATA (-8190).
  if (My_Decrypt(aes_key.get(), CKM_AES_GCM, &param,
                 output, &output_len, ciphertext.length(),
                 reinterpret_cast<const unsigned char*>(ciphertext.data()),
                 ciphertext.length()) != SECSuccess) {
    DLOG(INFO) << "My_Decrypt failed: NSS error " << PORT_GetError();
    return false;
  }

  if (output_len != plaintext_size) {
    DLOG(INFO) << "Wrong output length";
    return false;
  }
  *output_length = output_len;
  return true;
}

QuicData* Aes128GcmDecrypter::DecryptPacket(
    QuicPacketSequenceNumber sequence_number,
    StringPiece associated_data,
    StringPiece ciphertext) {
  COMPILE_ASSERT(sizeof(nonce_) == kNoncePrefixSize + sizeof(sequence_number),
                 incorrect_nonce_size);
  if (ciphertext.length() < kAuthTagSize) {
    return NULL;
  }
  size_t plaintext_size;
  scoped_ptr<char[]> plaintext(new char[ciphertext.length()]);

  memcpy(nonce_ + kNoncePrefixSize, &sequence_number, sizeof(sequence_number));
  if (!Decrypt(StringPiece(reinterpret_cast<char*>(nonce_), sizeof(nonce_)),
               associated_data, ciphertext,
               reinterpret_cast<unsigned char*>(plaintext.get()),
               &plaintext_size)) {
    return NULL;
  }
  return new QuicData(plaintext.release(), plaintext_size, true);
}

StringPiece Aes128GcmDecrypter::GetKey() const {
  return StringPiece(reinterpret_cast<const char*>(key_), sizeof(key_));
}

StringPiece Aes128GcmDecrypter::GetNoncePrefix() const {
  return StringPiece(reinterpret_cast<const char*>(nonce_), kNoncePrefixSize);
}

}  // namespace net
