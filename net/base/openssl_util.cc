// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/openssl_util.h"

#include <openssl/err.h>

#include "base/logging.h"
#include "base/platform_thread.h"

namespace net {

namespace {

// We do certificate verification after handshake, so we disable the default
// by registering a no-op verify function.
int NoOpVerifyCallback(X509_STORE_CTX*, void *) {
  DVLOG(3) << "skipping cert verify";
  return 1;
}

unsigned long CurrentThreadId() {
  return static_cast<unsigned long>(PlatformThread::CurrentId());
}

SSL_CTX* CreateSSL_CTX() {
  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  return SSL_CTX_new(SSLv23_client_method());
}

}  // namespace

OpenSSLInitSingleton::OpenSSLInitSingleton()
    : ssl_ctx_(CreateSSL_CTX()),
      store_(X509_STORE_new()) {
  CHECK(ssl_ctx_.get());
  CHECK(store_.get());

  SSL_CTX_set_cert_verify_callback(ssl_ctx_.get(), NoOpVerifyCallback, NULL);
  X509_STORE_set_default_paths(store_.get());
  // TODO(bulach): Enable CRL (see X509_STORE_set_flags(X509_V_FLAG_CRL_CHECK)).
  int num_locks = CRYPTO_num_locks();
  for (int i = 0; i < num_locks; ++i)
    locks_.push_back(new Lock());
  CRYPTO_set_locking_callback(LockingCallback);
  CRYPTO_set_id_callback(CurrentThreadId);
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

