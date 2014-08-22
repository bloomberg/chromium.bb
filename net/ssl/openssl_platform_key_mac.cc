// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/openssl_platform_key.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

#include <Security/cssm.h>
#include <Security/SecBase.h>
#include <Security/SecCertificate.h>
#include <Security/SecIdentity.h>
#include <Security/SecKey.h>

#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_policy.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "crypto/mac_security_services_lock.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/openssl_ssl_util.h"

namespace net {

namespace {

class ScopedCSSM_CC_HANDLE {
 public:
  ScopedCSSM_CC_HANDLE() : handle_(0) {
  }

  ~ScopedCSSM_CC_HANDLE() {
    reset();
  }

  CSSM_CC_HANDLE get() const {
    return handle_;
  }

  void reset() {
    if (handle_)
      CSSM_DeleteContext(handle_);
    handle_ = 0;
  }

  CSSM_CC_HANDLE* InitializeInto() {
    reset();
    return &handle_;
  }
 private:
  CSSM_CC_HANDLE handle_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCSSM_CC_HANDLE);
};

// Looks up the private key for |certificate| in KeyChain and returns
// a SecKeyRef or NULL on failure. The caller takes ownership of the
// result.
SecKeyRef FetchSecKeyRefForCertificate(const X509Certificate* certificate) {
  OSStatus status;
  base::ScopedCFTypeRef<SecIdentityRef> identity;
  {
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    status = SecIdentityCreateWithCertificate(
        NULL, certificate->os_cert_handle(), identity.InitializeInto());
  }
  if (status != noErr) {
    OSSTATUS_LOG(WARNING, status);
    return NULL;
  }

  base::ScopedCFTypeRef<SecKeyRef> private_key;
  status = SecIdentityCopyPrivateKey(identity, private_key.InitializeInto());
  if (status != noErr) {
    OSSTATUS_LOG(WARNING, status);
    return NULL;
  }

  return private_key.release();
}

extern const RSA_METHOD mac_rsa_method;
extern const ECDSA_METHOD mac_ecdsa_method;

// KeyExData contains the data that is contained in the EX_DATA of the
// RSA and ECDSA objects that are created to wrap Mac system keys.
struct KeyExData {
  KeyExData(SecKeyRef key, const CSSM_KEY* cssm_key)
      : key(key, base::scoped_policy::RETAIN), cssm_key(cssm_key) {}

  base::ScopedCFTypeRef<SecKeyRef> key;
  const CSSM_KEY* cssm_key;
};

// ExDataDup is called when one of the RSA or EC_KEY objects is
// duplicated. This is not supported and should never happen.
int ExDataDup(CRYPTO_EX_DATA* to,
              const CRYPTO_EX_DATA* from,
              void** from_d,
              int idx,
              long argl,
              void* argp) {
  CHECK(false);
  return 0;
}

// ExDataFree is called when one of the RSA or EC_KEY objects is freed.
void ExDataFree(void* parent,
                void* ptr,
                CRYPTO_EX_DATA* ex_data,
                int idx,
                long argl, void* argp) {
  KeyExData* data = reinterpret_cast<KeyExData*>(ptr);
  delete data;
}

// BoringSSLEngine is a BoringSSL ENGINE that implements RSA and ECDSA
// by forwarding the requested operations to Apple's CSSM
// implementation.
class BoringSSLEngine {
 public:
  BoringSSLEngine()
      : rsa_index_(RSA_get_ex_new_index(0 /* argl */,
                                        NULL /* argp */,
                                        NULL /* new_func */,
                                        ExDataDup,
                                        ExDataFree)),
        ec_key_index_(EC_KEY_get_ex_new_index(0 /* argl */,
                                              NULL /* argp */,
                                              NULL /* new_func */,
                                              ExDataDup,
                                              ExDataFree)),
        engine_(ENGINE_new()) {
    ENGINE_set_RSA_method(
        engine_, &mac_rsa_method, sizeof(mac_rsa_method));
    ENGINE_set_ECDSA_method(
        engine_, &mac_ecdsa_method, sizeof(mac_ecdsa_method));
  }

  int rsa_ex_index() const { return rsa_index_; }
  int ec_key_ex_index() const { return ec_key_index_; }

  const ENGINE* engine() const { return engine_; }

 private:
  const int rsa_index_;
  const int ec_key_index_;
  ENGINE* const engine_;
};

base::LazyInstance<BoringSSLEngine>::Leaky global_boringssl_engine =
    LAZY_INSTANCE_INITIALIZER;

// Helper function for making a signature.

// MakeCSSMSignature uses the key information in |ex_data| to sign the
// |in_len| bytes pointed by |in|. It writes up to |max_out| bytes
// into the buffer pointed to by |out|, setting |*out_len| to the
// number of bytes written. It returns 1 on success and 0 on failure.
int MakeCSSMSignature(const KeyExData* ex_data,
                      size_t* out_len,
                      uint8_t* out,
                      size_t max_out,
                      const uint8_t* in,
                      size_t in_len) {
  CSSM_CSP_HANDLE csp_handle;
  OSStatus status = SecKeyGetCSPHandle(ex_data->key.get(), &csp_handle);
  if (status != noErr) {
    OSSTATUS_LOG(WARNING, status);
    OPENSSL_PUT_ERROR(RSA, sign_raw, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  const CSSM_ACCESS_CREDENTIALS* cssm_creds = NULL;
  status = SecKeyGetCredentials(ex_data->key.get(), CSSM_ACL_AUTHORIZATION_SIGN,
                                kSecCredentialTypeDefault, &cssm_creds);
  if (status != noErr) {
    OSSTATUS_LOG(WARNING, status);
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return 0;
  }

  ScopedCSSM_CC_HANDLE cssm_signature;
  if (CSSM_CSP_CreateSignatureContext(
          csp_handle, ex_data->cssm_key->KeyHeader.AlgorithmId, cssm_creds,
          ex_data->cssm_key, cssm_signature.InitializeInto()) != CSSM_OK) {
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return 0;
  }

  if (ex_data->cssm_key->KeyHeader.AlgorithmId == CSSM_ALGID_RSA) {
    // Set RSA blinding.
    CSSM_CONTEXT_ATTRIBUTE blinding_attr;
    blinding_attr.AttributeType   = CSSM_ATTRIBUTE_RSA_BLINDING;
    blinding_attr.AttributeLength = sizeof(uint32);
    blinding_attr.Attribute.Uint32 = 1;
    if (CSSM_UpdateContextAttributes(
            cssm_signature.get(), 1, &blinding_attr) != CSSM_OK) {
      OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
      return 0;
    }
  }

  CSSM_DATA hash_data;
  hash_data.Length = in_len;
  hash_data.Data = const_cast<uint8*>(in);

  CSSM_DATA signature_data;
  signature_data.Length = max_out;
  signature_data.Data = out;

  if (CSSM_SignData(cssm_signature.get(), &hash_data, 1,
                    CSSM_ALGID_NONE, &signature_data) != CSSM_OK) {
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return 0;
  }

  *out_len = signature_data.Length;
  return 1;
}

// Custom RSA_METHOD that uses the platform APIs for signing.

const KeyExData* RsaGetExData(const RSA* rsa) {
  return reinterpret_cast<const KeyExData*>(
      RSA_get_ex_data(rsa, global_boringssl_engine.Get().rsa_ex_index()));
}

size_t RsaMethodSize(const RSA *rsa) {
  const KeyExData *ex_data = RsaGetExData(rsa);
  return (ex_data->cssm_key->KeyHeader.LogicalKeySizeInBits + 7) / 8;
}

int RsaMethodEncrypt(RSA* rsa,
                     size_t* out_len,
                     uint8_t* out,
                     size_t max_out,
                     const uint8_t* in,
                     size_t in_len,
                     int padding) {
  NOTIMPLEMENTED();
  OPENSSL_PUT_ERROR(RSA, encrypt, RSA_R_UNKNOWN_ALGORITHM_TYPE);
  return 0;
}

int RsaMethodSignRaw(RSA* rsa,
                     size_t* out_len,
                     uint8_t* out,
                     size_t max_out,
                     const uint8_t* in,
                     size_t in_len,
                     int padding) {
  // Only support PKCS#1 padding.
  DCHECK_EQ(RSA_PKCS1_PADDING, padding);
  if (padding != RSA_PKCS1_PADDING) {
    OPENSSL_PUT_ERROR(RSA, sign_raw, RSA_R_UNKNOWN_PADDING_TYPE);
    return 0;
  }

  const KeyExData *ex_data = RsaGetExData(rsa);
  if (!ex_data) {
    OPENSSL_PUT_ERROR(RSA, sign_raw, ERR_R_INTERNAL_ERROR);
    return 0;
  }
  DCHECK_EQ(CSSM_ALGID_RSA, ex_data->cssm_key->KeyHeader.AlgorithmId);

  return MakeCSSMSignature(ex_data, out_len, out, max_out, in, in_len);
}

int RsaMethodDecrypt(RSA* rsa,
                     size_t* out_len,
                     uint8_t* out,
                     size_t max_out,
                     const uint8_t* in,
                     size_t in_len,
                     int padding) {
  NOTIMPLEMENTED();
  OPENSSL_PUT_ERROR(RSA, decrypt, RSA_R_UNKNOWN_ALGORITHM_TYPE);
  return 0;
}

int RsaMethodVerifyRaw(RSA* rsa,
                       size_t* out_len,
                       uint8_t* out,
                       size_t max_out,
                       const uint8_t* in,
                       size_t in_len,
                       int padding) {
  NOTIMPLEMENTED();
  OPENSSL_PUT_ERROR(RSA, verify_raw, RSA_R_UNKNOWN_ALGORITHM_TYPE);
  return 0;
}

const RSA_METHOD mac_rsa_method = {
    {
     0 /* references */,
     1 /* is_static */
    } /* common */,
    NULL /* app_data */,

    NULL /* init */,
    NULL /* finish */,
    RsaMethodSize,
    NULL /* sign */,
    NULL /* verify */,
    RsaMethodEncrypt,
    RsaMethodSignRaw,
    RsaMethodDecrypt,
    RsaMethodVerifyRaw,
    NULL /* private_transform */,
    NULL /* mod_exp */,
    NULL /* bn_mod_exp */,
    RSA_FLAG_OPAQUE,
    NULL /* keygen */,
};

crypto::ScopedEVP_PKEY CreateRSAWrapper(SecKeyRef key,
                                        const CSSM_KEY* cssm_key) {
  crypto::ScopedRSA rsa(
      RSA_new_method(global_boringssl_engine.Get().engine()));
  if (!rsa)
    return crypto::ScopedEVP_PKEY();

  RSA_set_ex_data(
      rsa.get(), global_boringssl_engine.Get().rsa_ex_index(),
      new KeyExData(key, cssm_key));

  crypto::ScopedEVP_PKEY pkey(EVP_PKEY_new());
  if (!pkey)
    return crypto::ScopedEVP_PKEY();

  if (!EVP_PKEY_set1_RSA(pkey.get(), rsa.get()))
    return crypto::ScopedEVP_PKEY();

  return pkey.Pass();
}

// Custom ECDSA_METHOD that uses the platform APIs.
// Note that for now, only signing through ECDSA_sign() is really supported.
// all other method pointers are either stubs returning errors, or no-ops.

const KeyExData* EcKeyGetExData(const EC_KEY* ec_key) {
  return reinterpret_cast<const KeyExData*>(EC_KEY_get_ex_data(
      ec_key, global_boringssl_engine.Get().ec_key_ex_index()));
}

size_t EcdsaMethodGroupOrderSize(const EC_KEY* ec_key) {
  const KeyExData* ex_data = EcKeyGetExData(ec_key);
  // LogicalKeySizeInBits is the size of an EC public key. But an
  // ECDSA signature length depends on the size of the base point's
  // order. For P-256, P-384, and P-521, these two sizes are the same.
  return (ex_data->cssm_key->KeyHeader.LogicalKeySizeInBits + 7) / 8;
}

int EcdsaMethodSign(const uint8_t* digest,
                    size_t digest_len,
                    uint8_t* sig,
                    unsigned int* sig_len,
                    EC_KEY* ec_key) {
  const KeyExData *ex_data = EcKeyGetExData(ec_key);
  if (!ex_data) {
    OPENSSL_PUT_ERROR(RSA, sign_raw, ERR_R_INTERNAL_ERROR);
    return 0;
  }
  DCHECK_EQ(CSSM_ALGID_ECDSA, ex_data->cssm_key->KeyHeader.AlgorithmId);

  // TODO(davidben): Fix BoringSSL to make sig_len a size_t*.
  size_t out_len;
  int ret = MakeCSSMSignature(
      ex_data, &out_len, sig, ECDSA_size(ec_key), digest, digest_len);
  if (!ret)
    return 0;
  *sig_len = out_len;
  return 1;
}

int EcdsaMethodVerify(const uint8_t* digest,
                      size_t digest_len,
                      const uint8_t* sig,
                      size_t sig_len,
                      EC_KEY* eckey) {
  NOTIMPLEMENTED();
  OPENSSL_PUT_ERROR(ECDSA, ECDSA_do_verify, ECDSA_R_NOT_IMPLEMENTED);
  return 0;
}

const ECDSA_METHOD mac_ecdsa_method = {
    {
     0 /* references */,
     1 /* is_static */
    } /* common */,
    NULL /* app_data */,

    NULL /* init */,
    NULL /* finish */,
    EcdsaMethodGroupOrderSize,
    EcdsaMethodSign,
    EcdsaMethodVerify,
    ECDSA_FLAG_OPAQUE,
};

crypto::ScopedEVP_PKEY CreateECDSAWrapper(SecKeyRef key,
                                          const CSSM_KEY* cssm_key) {
  crypto::ScopedEC_KEY ec_key(
      EC_KEY_new_method(global_boringssl_engine.Get().engine()));
  if (!ec_key)
    return crypto::ScopedEVP_PKEY();

  EC_KEY_set_ex_data(
      ec_key.get(), global_boringssl_engine.Get().ec_key_ex_index(),
      new KeyExData(key, cssm_key));

  crypto::ScopedEVP_PKEY pkey(EVP_PKEY_new());
  if (!pkey)
    return crypto::ScopedEVP_PKEY();

  if (!EVP_PKEY_set1_EC_KEY(pkey.get(), ec_key.get()))
    return crypto::ScopedEVP_PKEY();

  return pkey.Pass();
}

crypto::ScopedEVP_PKEY CreatePkeyWrapper(SecKeyRef key) {
  const CSSM_KEY* cssm_key;
  OSStatus status = SecKeyGetCSSMKey(key, &cssm_key);
  if (status != noErr)
    return crypto::ScopedEVP_PKEY();

  switch (cssm_key->KeyHeader.AlgorithmId) {
    case CSSM_ALGID_RSA:
      return CreateRSAWrapper(key, cssm_key);
    case CSSM_ALGID_ECDSA:
      return CreateECDSAWrapper(key, cssm_key);
    default:
      // TODO(davidben): Filter out anything other than ECDSA and RSA
      // elsewhere. We don't support other key types.
      NOTREACHED();
      LOG(ERROR) << "Unknown key type";
      return crypto::ScopedEVP_PKEY();
  }
}

}  // namespace

crypto::ScopedEVP_PKEY FetchClientCertPrivateKey(
    const X509Certificate* certificate) {
  // Look up the private key.
  base::ScopedCFTypeRef<SecKeyRef> private_key(
      FetchSecKeyRefForCertificate(certificate));
  if (!private_key)
    return crypto::ScopedEVP_PKEY();

  // Create an EVP_PKEY wrapper.
  return CreatePkeyWrapper(private_key.get());
}

}  // namespace net
