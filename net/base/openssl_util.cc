// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/openssl_util.h"

#include <openssl/err.h>
#include <openssl/x509v3.h>

#include "base/logging.h"

namespace net {

X509_STORE* OpenSSLInitSingleton::x509_store() const {
  return store_.get();
}

OpenSSLInitSingleton::OpenSSLInitSingleton()
    : store_(X509_STORE_new()) {
  CHECK(store_.get());
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();
  X509_STORE_set_default_paths(store_.get());
  // TODO(bulach): Enable CRL (see X509_STORE_set_flags(X509_V_FLAG_CRL_CHECK)).
  int num_locks = CRYPTO_num_locks();
  for (int i = 0; i < num_locks; ++i)
    locks_.push_back(new Lock());
  CRYPTO_set_locking_callback(LockingCallback);
}

OpenSSLInitSingleton::~OpenSSLInitSingleton() {
  CRYPTO_set_locking_callback(NULL);
  EVP_cleanup();
  ERR_free_strings();
}

OpenSSLInitSingleton* GetOpenSSLInitSingleton() {
  return Singleton<OpenSSLInitSingleton>::get();
}

// static
void OpenSSLInitSingleton::LockingCallback(int mode,
                                            int n,
                                            const char* file,
                                            int line) {
  GetOpenSSLInitSingleton()->OnLockingCallback(mode, n, file, line);
}

void OpenSSLInitSingleton::OnLockingCallback(int mode,
                                              int n,
                                              const char* file,
                                              int line) {
  CHECK_LT(static_cast<size_t>(n), locks_.size());
  if (mode & CRYPTO_LOCK)
    locks_[n]->Acquire();
  else
    locks_[n]->Release();
}

}  // namespace net

