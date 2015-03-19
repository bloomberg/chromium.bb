// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/openssl_platform_key.h"

#include <windows.h>
#include <NCrypt.h>

#include <string.h>

#include <algorithm>
#include <vector>

#include <openssl/bn.h>
#include <openssl/digest.h>
#include <openssl/ec_key.h>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/obj_mac.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/profiler/scoped_tracker.h"
#include "base/win/windows_version.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_capi_types.h"
#include "crypto/wincrypt_shim.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/openssl_ssl_util.h"
#include "net/ssl/scoped_openssl_types.h"

namespace net {

namespace {

using NCryptFreeObjectFunc = SECURITY_STATUS(WINAPI*)(NCRYPT_HANDLE);
using NCryptSignHashFunc =
    SECURITY_STATUS(WINAPI*)(NCRYPT_KEY_HANDLE,  // hKey
                             VOID*,  // pPaddingInfo
                             PBYTE,  // pbHashValue
                             DWORD,  // cbHashValue
                             PBYTE,  // pbSignature
                             DWORD,  // cbSignature
                             DWORD*,  // pcbResult
                             DWORD);  // dwFlags

class CNGFunctions {
 public:
  CNGFunctions()
      : ncrypt_free_object_(nullptr),
        ncrypt_sign_hash_(nullptr) {
    HMODULE ncrypt = GetModuleHandle(L"ncrypt.dll");
    if (ncrypt != nullptr) {
      ncrypt_free_object_ = reinterpret_cast<NCryptFreeObjectFunc>(
          GetProcAddress(ncrypt, "NCryptFreeObject"));
      ncrypt_sign_hash_ = reinterpret_cast<NCryptSignHashFunc>(
          GetProcAddress(ncrypt, "NCryptSignHash"));
    }
  }

  NCryptFreeObjectFunc ncrypt_free_object() const {
    return ncrypt_free_object_;
  }

  NCryptSignHashFunc ncrypt_sign_hash() const { return ncrypt_sign_hash_; }

 private:
  NCryptFreeObjectFunc ncrypt_free_object_;
  NCryptSignHashFunc ncrypt_sign_hash_;
};

base::LazyInstance<CNGFunctions>::Leaky g_cng_functions =
    LAZY_INSTANCE_INITIALIZER;

struct CERT_KEY_CONTEXTDeleter {
  void operator()(PCERT_KEY_CONTEXT key) {
    if (key->dwKeySpec == CERT_NCRYPT_KEY_SPEC) {
      g_cng_functions.Get().ncrypt_free_object()(key->hNCryptKey);
    } else {
      CryptReleaseContext(key->hCryptProv, 0);
    }
    delete key;
  }
};

using ScopedCERT_KEY_CONTEXT =
    scoped_ptr<CERT_KEY_CONTEXT, CERT_KEY_CONTEXTDeleter>;

// KeyExData contains the data that is contained in the EX_DATA of the
// RSA and ECDSA objects that are created to wrap Windows system keys.
struct KeyExData {
  KeyExData(ScopedCERT_KEY_CONTEXT key, size_t key_length)
      : key(key.Pass()), key_length(key_length) {}

  ScopedCERT_KEY_CONTEXT key;
  size_t key_length;
};

// ExDataDup is called when one of the RSA or EC_KEY objects is
// duplicated. This is not supported and should never happen.
int ExDataDup(CRYPTO_EX_DATA* to,
              const CRYPTO_EX_DATA* from,
              void** from_d,
              int idx,
              long argl,
              void* argp) {
  CHECK_EQ((void*)nullptr, *from_d);
  return 0;
}

// ExDataFree is called when one of the RSA or EC_KEY objects is freed.
void ExDataFree(void* parent,
                void* ptr,
                CRYPTO_EX_DATA* ex_data,
                int idx,
                long argl,
                void* argp) {
  KeyExData* data = reinterpret_cast<KeyExData*>(ptr);
  delete data;
}

extern const RSA_METHOD win_rsa_method;
extern const ECDSA_METHOD win_ecdsa_method;

// BoringSSLEngine is a BoringSSL ENGINE that implements RSA and ECDSA
// by forwarding the requested operations to CAPI or CNG.
class BoringSSLEngine {
 public:
  BoringSSLEngine()
      : rsa_index_(RSA_get_ex_new_index(0 /* argl */,
                                        nullptr /* argp */,
                                        nullptr /* new_func */,
                                        ExDataDup,
                                        ExDataFree)),
        ec_key_index_(EC_KEY_get_ex_new_index(0 /* argl */,
                                              nullptr /* argp */,
                                              nullptr /* new_func */,
                                              ExDataDup,
                                              ExDataFree)),
        engine_(ENGINE_new()) {
    ENGINE_set_RSA_method(engine_, &win_rsa_method, sizeof(win_rsa_method));
    ENGINE_set_ECDSA_method(engine_, &win_ecdsa_method,
                            sizeof(win_ecdsa_method));
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

// Custom RSA_METHOD that uses the platform APIs for signing.

const KeyExData* RsaGetExData(const RSA* rsa) {
  return reinterpret_cast<const KeyExData*>(
      RSA_get_ex_data(rsa, global_boringssl_engine.Get().rsa_ex_index()));
}

size_t RsaMethodSize(const RSA* rsa) {
  const KeyExData* ex_data = RsaGetExData(rsa);
  return (ex_data->key_length + 7) / 8;
}

int RsaMethodSign(int hash_nid,
                  const uint8_t* in,
                  unsigned in_len,
                  uint8_t* out,
                  unsigned* out_len,
                  const RSA* rsa) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION("424386 RsaMethodSign"));

  // TODO(davidben): Switch BoringSSL's sign hook to using size_t rather than
  // unsigned.
  const KeyExData* ex_data = RsaGetExData(rsa);
  if (!ex_data) {
    NOTREACHED();
    OPENSSL_PUT_ERROR(RSA, RSA_sign, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  if (ex_data->key->dwKeySpec == CERT_NCRYPT_KEY_SPEC) {
    BCRYPT_PKCS1_PADDING_INFO rsa_padding_info;
    switch (hash_nid) {
      case NID_md5_sha1:
        rsa_padding_info.pszAlgId = nullptr;
        break;
      case NID_sha1:
        rsa_padding_info.pszAlgId = BCRYPT_SHA1_ALGORITHM;
        break;
      case NID_sha256:
        rsa_padding_info.pszAlgId = BCRYPT_SHA256_ALGORITHM;
        break;
      case NID_sha384:
        rsa_padding_info.pszAlgId = BCRYPT_SHA384_ALGORITHM;
        break;
      case NID_sha512:
        rsa_padding_info.pszAlgId = BCRYPT_SHA512_ALGORITHM;
        break;
      default:
        OPENSSL_PUT_ERROR(RSA, RSA_sign, RSA_R_UNKNOWN_ALGORITHM_TYPE);
        return 0;
    }

    DWORD signature_len;
    SECURITY_STATUS ncrypt_status = g_cng_functions.Get().ncrypt_sign_hash()(
        ex_data->key->hNCryptKey, &rsa_padding_info, const_cast<PBYTE>(in),
        in_len, out, RSA_size(rsa), &signature_len, BCRYPT_PAD_PKCS1);
    if (FAILED(ncrypt_status) || signature_len == 0) {
      OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
      return 0;
    }
    *out_len = signature_len;
    return 1;
  }

  ALG_ID hash_alg;
  switch (hash_nid) {
    case NID_md5_sha1:
      hash_alg = CALG_SSL3_SHAMD5;
      break;
    case NID_sha1:
      hash_alg = CALG_SHA1;
      break;
    case NID_sha256:
      hash_alg = CALG_SHA_256;
      break;
    case NID_sha384:
      hash_alg = CALG_SHA_384;
      break;
    case NID_sha512:
      hash_alg = CALG_SHA_512;
      break;
    default:
      OPENSSL_PUT_ERROR(RSA, RSA_sign, RSA_R_UNKNOWN_ALGORITHM_TYPE);
      return 0;
  }

  HCRYPTHASH hash;
  if (!CryptCreateHash(ex_data->key->hCryptProv, hash_alg, 0, 0, &hash)) {
    PLOG(ERROR) << "CreateCreateHash failed";
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return 0;
  }
  DWORD hash_len;
  DWORD arg_len = sizeof(hash_len);
  if (!CryptGetHashParam(hash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&hash_len),
                         &arg_len, 0)) {
    PLOG(ERROR) << "CryptGetHashParam HP_HASHSIZE failed";
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return 0;
  }
  if (hash_len != in_len) {
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return 0;
  }
  if (!CryptSetHashParam(hash, HP_HASHVAL, const_cast<BYTE*>(in), 0)) {
    PLOG(ERROR) << "CryptSetHashParam HP_HASHVAL failed";
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return 0;
  }
  DWORD signature_len = RSA_size(rsa);
  if (!CryptSignHash(hash, ex_data->key->dwKeySpec, nullptr, 0, out,
                     &signature_len)) {
    PLOG(ERROR) << "CryptSignHash failed";
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return 0;
  }

  /* CryptoAPI signs in little-endian, so reverse it. */
  std::reverse(out, out + signature_len);
  *out_len = signature_len;
  return 1;
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
  NOTIMPLEMENTED();
  OPENSSL_PUT_ERROR(RSA, encrypt, RSA_R_UNKNOWN_ALGORITHM_TYPE);
  return 0;
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

int RsaMethodSupportsDigest(const RSA* rsa, const EVP_MD* md) {
  const KeyExData* ex_data = RsaGetExData(rsa);
  if (!ex_data) {
    NOTREACHED();
    return 0;
  }

  int hash_nid = EVP_MD_type(md);
  if (ex_data->key->dwKeySpec == CERT_NCRYPT_KEY_SPEC) {
    // Only hashes which appear in RsaSignPKCS1 are supported.
    if (hash_nid != NID_sha1 && hash_nid != NID_sha256 &&
        hash_nid != NID_sha384 && hash_nid != NID_sha512) {
      return 0;
    }

    // If the key is a 1024-bit RSA, assume conservatively that it may only be
    // able to sign SHA-1 hashes. This is the case for older Estonian ID cards
    // that have 1024-bit RSA keys.
    //
    // CNG does provide NCryptIsAlgSupported and NCryptEnumAlgorithms functions,
    // however they seem to both return NTE_NOT_SUPPORTED when querying the
    // NCRYPT_PROV_HANDLE at the key's NCRYPT_PROVIDER_HANDLE_PROPERTY.
    if (ex_data->key_length <= 1024 && hash_nid != NID_sha1)
      return 0;

    return 1;
  } else {
    // If the key is in CAPI, assume conservatively that the CAPI service
    // provider may only be able to sign SHA-1 hashes.
    return hash_nid == NID_sha1;
  }
}

const RSA_METHOD win_rsa_method = {
    {
     0,  // references
     1,  // is_static
    },
    nullptr,  // app_data

    nullptr,  // init
    nullptr,  // finish
    RsaMethodSize,
    RsaMethodSign,
    nullptr,  // verify
    RsaMethodEncrypt,
    RsaMethodSignRaw,
    RsaMethodDecrypt,
    RsaMethodVerifyRaw,
    nullptr,  // private_transform
    nullptr,  // mod_exp
    nullptr,  // bn_mod_exp
    RSA_FLAG_OPAQUE,
    nullptr,  // keygen
    RsaMethodSupportsDigest,
};

// Custom ECDSA_METHOD that uses the platform APIs.
// Note that for now, only signing through ECDSA_sign() is really supported.
// all other method pointers are either stubs returning errors, or no-ops.

const KeyExData* EcKeyGetExData(const EC_KEY* ec_key) {
  return reinterpret_cast<const KeyExData*>(EC_KEY_get_ex_data(
      ec_key, global_boringssl_engine.Get().ec_key_ex_index()));
}

size_t EcdsaMethodGroupOrderSize(const EC_KEY* ec_key) {
  const KeyExData* ex_data = EcKeyGetExData(ec_key);
  // key_length is the size of the group order for EC keys.
  return (ex_data->key_length + 7) / 8;
}

int EcdsaMethodSign(const uint8_t* digest,
                    size_t digest_len,
                    uint8_t* out_sig,
                    unsigned int* out_sig_len,
                    EC_KEY* ec_key) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION("424386 EcdsaMethodSign"));

  const KeyExData* ex_data = EcKeyGetExData(ec_key);
  // Only CNG supports ECDSA.
  if (!ex_data || ex_data->key->dwKeySpec != CERT_NCRYPT_KEY_SPEC) {
    NOTREACHED();
    OPENSSL_PUT_ERROR(RSA, sign_raw, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  // An ECDSA signature is two integers, modulo the order of the group.
  size_t order_len = (ex_data->key_length + 7) / 8;
  if (order_len == 0) {
    NOTREACHED();
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return 0;
  }
  std::vector<uint8_t> raw_sig(order_len * 2);

  DWORD signature_len;
  SECURITY_STATUS ncrypt_status = g_cng_functions.Get().ncrypt_sign_hash()(
      ex_data->key->hNCryptKey, nullptr, const_cast<PBYTE>(digest), digest_len,
      &raw_sig[0], raw_sig.size(), &signature_len, 0);
  if (FAILED(ncrypt_status) || signature_len != raw_sig.size()) {
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return 0;
  }

  // Convert the RAW ECDSA signature to a DER-encoded ECDSA-Sig-Value.
  crypto::ScopedECDSA_SIG sig(ECDSA_SIG_new());
  if (!sig) {
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return 0;
  }
  sig->r = BN_bin2bn(&raw_sig[0], order_len, nullptr);
  sig->s = BN_bin2bn(&raw_sig[order_len], order_len, nullptr);
  if (!sig->r || !sig->s) {
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return 0;
  }

  // Ensure the DER-encoded signature fits in the bounds.
  int len = i2d_ECDSA_SIG(sig.get(), nullptr);
  if (len < 0 || static_cast<size_t>(len) > ECDSA_size(ec_key)) {
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return 0;
  }

  len = i2d_ECDSA_SIG(sig.get(), &out_sig);
  if (len < 0) {
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return 0;
  }
  *out_sig_len = len;
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

const ECDSA_METHOD win_ecdsa_method = {
    {
     0,  // references
     1,  // is_static
    },
    nullptr,  // app_data

    nullptr,  // init
    nullptr,  // finish
    EcdsaMethodGroupOrderSize,
    EcdsaMethodSign,
    EcdsaMethodVerify,
    ECDSA_FLAG_OPAQUE,
};

// Determines the key type and length of |certificate|'s public key. The type is
// returned as an OpenSSL EVP_PKEY type. The key length for RSA key is the size
// of the RSA modulus in bits. For an ECDSA key, it is the number of bits to
// represent the group order. It returns true on success and false on failure.
bool GetKeyInfo(const X509Certificate* certificate,
                int* out_type,
                size_t* out_length) {
  crypto::OpenSSLErrStackTracer tracker(FROM_HERE);

  std::string der_encoded;
  if (!X509Certificate::GetDEREncoded(certificate->os_cert_handle(),
                                      &der_encoded))
    return false;
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(der_encoded.data());
  ScopedX509 x509(d2i_X509(NULL, &bytes, der_encoded.size()));
  if (!x509)
    return false;
  crypto::ScopedEVP_PKEY key(X509_get_pubkey(x509.get()));
  if (!key)
    return false;
  *out_type = EVP_PKEY_id(key.get());
  *out_length = EVP_PKEY_bits(key.get());
  return true;
}

crypto::ScopedEVP_PKEY CreateRSAWrapper(ScopedCERT_KEY_CONTEXT key,
                                        size_t key_length) {
  crypto::ScopedRSA rsa(RSA_new_method(global_boringssl_engine.Get().engine()));
  if (!rsa)
    return nullptr;

  RSA_set_ex_data(rsa.get(), global_boringssl_engine.Get().rsa_ex_index(),
                  new KeyExData(key.Pass(), key_length));

  crypto::ScopedEVP_PKEY pkey(EVP_PKEY_new());
  if (!pkey || !EVP_PKEY_set1_RSA(pkey.get(), rsa.get()))
    return nullptr;
  return pkey.Pass();
}

crypto::ScopedEVP_PKEY CreateECDSAWrapper(ScopedCERT_KEY_CONTEXT key,
                                          size_t key_length) {
  crypto::ScopedEC_KEY ec_key(
      EC_KEY_new_method(global_boringssl_engine.Get().engine()));
  if (!ec_key)
    return nullptr;

  EC_KEY_set_ex_data(ec_key.get(),
                     global_boringssl_engine.Get().ec_key_ex_index(),
                     new KeyExData(key.Pass(), key_length));

  crypto::ScopedEVP_PKEY pkey(EVP_PKEY_new());
  if (!pkey || !EVP_PKEY_set1_EC_KEY(pkey.get(), ec_key.get()))
    return nullptr;

  return pkey.Pass();
}

}  // namespace

crypto::ScopedEVP_PKEY FetchClientCertPrivateKey(
    const X509Certificate* certificate) {
  PCCERT_CONTEXT cert_context = certificate->os_cert_handle();

  HCRYPTPROV_OR_NCRYPT_KEY_HANDLE crypt_prov = 0;
  DWORD key_spec = 0;
  BOOL must_free = FALSE;
  DWORD flags = 0;
  if (base::win::GetVersion() >= base::win::VERSION_VISTA)
    flags |= CRYPT_ACQUIRE_PREFER_NCRYPT_KEY_FLAG;

  if (!CryptAcquireCertificatePrivateKey(cert_context, flags, nullptr,
                                         &crypt_prov, &key_spec, &must_free)) {
    PLOG(WARNING) << "Could not acquire private key";
    return nullptr;
  }

  // Should never get a cached handle back - ownership must always be
  // transferred.
  CHECK_EQ(must_free, TRUE);
  ScopedCERT_KEY_CONTEXT key(new CERT_KEY_CONTEXT);
  key->dwKeySpec = key_spec;
  key->hCryptProv = crypt_prov;

  // Rather than query the private key for metadata, extract the public key from
  // the certificate without using Windows APIs. CAPI and CNG do not
  // consistently work depending on the system. See https://crbug.com/468345.
  int key_type;
  size_t key_length;
  if (!GetKeyInfo(certificate, &key_type, &key_length))
    return nullptr;

  switch (key_type) {
    case EVP_PKEY_RSA:
      return CreateRSAWrapper(key.Pass(), key_length);
    case EVP_PKEY_EC:
      return CreateECDSAWrapper(key.Pass(), key_length);
    default:
      return nullptr;
  }
}

}  // namespace net
