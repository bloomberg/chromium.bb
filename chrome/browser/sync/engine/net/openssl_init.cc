// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// OpenSSL multi-threading initialization

#include "chrome/browser/sync/engine/net/openssl_init.h"

#include <openssl/crypto.h>

#include "base/logging.h"
#include "chrome/browser/sync/util/compat-pthread.h"
#include "chrome/browser/sync/util/pthread_helpers.h"

// OpenSSL requires multithreading callbacks to be initialized prior to using
// the library so that it can manage thread locking as necessary.

// Dynamic lock type
//
// This needs to be a struct and in global scope because OpenSSL relies on some
// macro magic.
struct CRYPTO_dynlock_value {
  PThreadMutex mutex;
  void Lock() {
    mutex.Lock();
  }
  void Unlock() {
    mutex.Unlock();
  }
};

namespace {

// This array stores all of the mutexes available to OpenSSL
PThreadMutex* mutex_buf = NULL;

// OpenSSL mutex handling callback functions

// OpenSSL Callback - Locks/unlocks the specified mutex held by OpenSSL.
void OpenSslMutexLockControl(int mode, int n, const char* file, int line) {
  if (mode & CRYPTO_LOCK) {
    mutex_buf[n].Lock();
  } else {
    mutex_buf[n].Unlock();
  }
}

// OpenSSL Callback - Returns the thread ID
unsigned long OpenSslGetThreadID(void) {
  return GetCurrentThreadId();
}

// Dynamic locking functions

// Allocate a new lock
struct CRYPTO_dynlock_value* dyn_create_function(const char* file, int line) {
  return new CRYPTO_dynlock_value;
}

void dyn_lock_function(int mode, struct CRYPTO_dynlock_value* lock,
                       const char* file, int line) {
  if (mode & CRYPTO_LOCK) {
    lock->Lock();
  } else {
    lock->Unlock();
  }
}

void dyn_destroy_function(struct CRYPTO_dynlock_value* lock,
                          const char* file, int line) {
  delete lock;
}

}  // namespace

// We want to log the version of the OpenSSL library being used, in particular
// for the case where it's dynamically linked. We want the version from the
// library, not from the header files. It seems the OpenSSL folks haven't
// bothered with an accessor for this, so we just pluck it out.
#ifdef OS_WINDOWS
// TODO(sync): Figure out how to get the SSL version string on Windows.
const char* SSL_version_str = "UNKNOWN";
#else
extern const char* SSL_version_str;
#endif

namespace browser_sync {

// Initializes the OpenSSL multithreading callbacks. This isn't thread-safe,
// but it is called early enough that it doesn't matter.
void InitOpenSslMultithreading() {
  LOG(INFO) << "Using OpenSSL headers version " << OPENSSL_VERSION_TEXT
            << ", lib version " << SSL_version_str;

  if (mutex_buf)
    return;

  mutex_buf = new PThreadMutex[CRYPTO_num_locks()];
  CHECK(NULL != mutex_buf);

  // OpenSSL has only one single global set of callbacks, so this
  // initialization must be done only once, even though the OpenSSL lib may be
  // used by multiple modules (jingle jabber connections and P2P tunnels).
  CRYPTO_set_id_callback(OpenSslGetThreadID);
  CRYPTO_set_locking_callback(OpenSslMutexLockControl);

  CRYPTO_set_dynlock_create_callback(dyn_create_function);
  CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
  CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);
}

// Cleans up the OpenSSL multithreading callbacks.
void CleanupOpenSslMultithreading() {
  if (!mutex_buf) {
    return;
  }

  CRYPTO_set_dynlock_create_callback(NULL);
  CRYPTO_set_dynlock_lock_callback(NULL);
  CRYPTO_set_dynlock_destroy_callback(NULL);

  CRYPTO_set_id_callback(NULL);
  CRYPTO_set_locking_callback(NULL);

  delete [] mutex_buf;
  mutex_buf = NULL;
}

}  // namespace browser_sync
