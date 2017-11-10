// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_platform_key_android.h"

#include <strings.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/scoped_java_ref.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "net/android/keystore.h"
#include "net/android/legacy_openssl.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_platform_key_util.h"
#include "net/ssl/threaded_ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace net {

namespace {

// On Android < 4.2, the libkeystore.so ENGINE uses CRYPTO_EX_DATA and is not
// added to the global engine list. If all references to it are dropped, OpenSSL
// will dlclose the module, leaving a dangling function pointer in the RSA
// CRYPTO_EX_DATA class. To work around this, leak an extra reference to the
// ENGINE we extract in GetRsaLegacyKey.
//
// In 4.2, this change avoids the problem:
// https://android.googlesource.com/platform/libcore/+/106a8928fb4249f2f3d4dba1dddbe73ca5cb3d61
//
// https://crbug.com/381465
class KeystoreEngineWorkaround {
 public:
  KeystoreEngineWorkaround() {}

  void LeakEngine(const JavaRef<jobject>& key) {
    if (!engine_.is_null())
      return;
    ScopedJavaLocalRef<jobject> engine =
        android::GetOpenSSLEngineForPrivateKey(key);
    if (engine.is_null()) {
      NOTREACHED();
      return;
    }
    engine_.Reset(engine);
  }

 private:
  ScopedJavaGlobalRef<jobject> engine_;
};

void LeakEngine(const JavaRef<jobject>& private_key) {
  static base::LazyInstance<KeystoreEngineWorkaround>::Leaky s_instance =
      LAZY_INSTANCE_INITIALIZER;
  s_instance.Get().LeakEngine(private_key);
}

class SSLPlatformKeyAndroid : public ThreadedSSLPrivateKey::Delegate {
 public:
  SSLPlatformKeyAndroid(int type,
                        const JavaRef<jobject>& key,
                        size_t max_length,
                        android::AndroidRSA* legacy_rsa)
      : type_(type), max_length_(max_length), legacy_rsa_(legacy_rsa) {
    key_.Reset(key);
  }

  ~SSLPlatformKeyAndroid() override {}

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return SSLPrivateKey::DefaultAlgorithmPreferences(type_,
                                                      false /* no PSS */);
  }

  Error Sign(uint16_t algorithm,
             base::span<const uint8_t> input,
             std::vector<uint8_t>* signature) override {
    // Hash the input.
    const EVP_MD* md = SSL_get_signature_algorithm_digest(algorithm);
    uint8_t digest[EVP_MAX_MD_SIZE];
    unsigned digest_len;
    if (!md || !EVP_Digest(input.data(), input.size(), digest, &digest_len, md,
                           nullptr)) {
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    input = base::make_span(digest, digest_len);

    // Prepend the DigestInfo for RSA.
    bssl::UniquePtr<uint8_t> digest_info_storage;
    if (SSL_get_signature_algorithm_key_type(algorithm) == EVP_PKEY_RSA) {
      int hash_nid = EVP_MD_type(md);
      uint8_t* digest_info;
      size_t digest_info_len;
      int is_alloced;
      if (!RSA_add_pkcs1_prefix(&digest_info, &digest_info_len, &is_alloced,
                                hash_nid, input.data(), input.size())) {
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      }

      if (is_alloced)
        digest_info_storage.reset(digest_info);
      input = base::make_span(digest_info, digest_info_len);
    }

    // Pre-4.2 legacy codepath.
    if (legacy_rsa_) {
      signature->resize(max_length_);
      int ret = legacy_rsa_->meth->rsa_priv_enc(
          input.size(), input.data(), signature->data(), legacy_rsa_,
          android::ANDROID_RSA_PKCS1_PADDING);
      if (ret < 0) {
        LOG(WARNING) << "Could not sign message with legacy RSA key!";
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      }
      signature->resize(ret);
      return OK;
    }

    if (!android::RawSignDigestWithPrivateKey(key_, input, signature)) {
      LOG(WARNING) << "Could not sign message with private key!";
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    return OK;
  }

 private:
  int type_;
  ScopedJavaGlobalRef<jobject> key_;
  size_t max_length_;
  android::AndroidRSA* legacy_rsa_;

  DISALLOW_COPY_AND_ASSIGN(SSLPlatformKeyAndroid);
};

}  // namespace

scoped_refptr<SSLPrivateKey> WrapJavaPrivateKey(
    const X509Certificate* certificate,
    const JavaRef<jobject>& key) {
  int type;
  size_t max_length;
  if (!GetClientCertInfo(certificate, &type, &max_length))
    return nullptr;

  android::AndroidRSA* sys_rsa = nullptr;
  if (type == EVP_PKEY_RSA) {
    const int kAndroid42ApiLevel = 17;
    if (base::android::BuildInfo::GetInstance()->sdk_int() <
        kAndroid42ApiLevel) {
      // Route around platform limitations: if Android < 4.2, then
      // base::android::RawSignDigestWithPrivateKey() cannot work, so try to get
      // the system OpenSSL's EVP_PKEY backing this PrivateKey object.
      android::AndroidEVP_PKEY* sys_pkey =
          android::GetOpenSSLSystemHandleForPrivateKey(key);
      if (!sys_pkey)
        return nullptr;

      if (sys_pkey->type != android::ANDROID_EVP_PKEY_RSA) {
        LOG(ERROR) << "Private key has wrong type!";
        return nullptr;
      }

      sys_rsa = sys_pkey->pkey.rsa;
      if (sys_rsa->engine) {
        // |private_key| may not have an engine if the PrivateKey did not come
        // from the key store, such as in unit tests.
        if (strcmp(sys_rsa->engine->id, "keystore") == 0) {
          LeakEngine(key);
        } else {
          NOTREACHED();
        }
      }
    }
  }

  return base::MakeRefCounted<ThreadedSSLPrivateKey>(
      std::make_unique<SSLPlatformKeyAndroid>(type, key, max_length, sys_rsa),
      GetSSLPlatformKeyTaskRunner());
}

}  // namespace net
