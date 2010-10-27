// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/ssl.h>

#include "base/lock.h"
#include "base/scoped_vector.h"
#include "base/singleton.h"

namespace net {

// A helper class that takes care of destroying OpenSSL objects when it goes out
// of scope.
template <typename T, void (*destructor)(T*)>
class ScopedSSL {
 public:
  explicit ScopedSSL(T* ptr_) : ptr_(ptr_) { }
  ~ScopedSSL() { if (ptr_) (*destructor)(ptr_); }

  T* get() const { return ptr_; }

 private:
  T* ptr_;
};

// Singleton for initializing / cleaning up OpenSSL and holding a X509 store.
// Access it via GetOpenSSLInitSingleton().
class OpenSSLInitSingleton {
 public:
  SSL_CTX* ssl_ctx() const { return ssl_ctx_.get(); }
  X509_STORE* x509_store() const { return store_.get(); }

 private:
  friend struct DefaultSingletonTraits<OpenSSLInitSingleton>;
  OpenSSLInitSingleton();
  ~OpenSSLInitSingleton();

  static void LockingCallback(int mode, int n, const char* file, int line);
  void OnLockingCallback(int mode, int n, const char* file, int line);

  ScopedSSL<SSL_CTX, SSL_CTX_free> ssl_ctx_;
  ScopedSSL<X509_STORE, X509_STORE_free> store_;
  // These locks are used and managed by OpenSSL via LockingCallback().
  ScopedVector<Lock> locks_;

  DISALLOW_COPY_AND_ASSIGN(OpenSSLInitSingleton);
};

OpenSSLInitSingleton* GetOpenSSLInitSingleton();

// Initialize OpenSSL if it isn't already initialized. This must be called
// before any other OpenSSL functions (except GetOpenSSLInitSingleton above).
// This function is thread-safe, and OpenSSL will only ever be initialized once.
// OpenSSL will be properly shut down on program exit.
void EnsureOpenSSLInit();

}  // namespace net

