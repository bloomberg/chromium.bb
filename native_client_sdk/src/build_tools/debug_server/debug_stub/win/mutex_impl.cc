/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <windows.h>
#include <exception>

#include "native_client/src/debug_server/port/mutex.h"

/*
 * Define the gdb_utils IMutex interface to use the NaCl version.
 * Unfortunately NaClSync does not have the correct recursive
 * property so we need to use our own version.
 */

namespace port {

class Mutex : public IMutex {
 public:
  Mutex() : handle_(NULL) {
    handle_ = CreateMutex(NULL, FALSE, NULL);

    // If we fail to create the mutex, then abort the app.
    if (NULL == handle_) throw std::exception("Failed to create mutex.");
  }

  ~Mutex() {
    // If constructor where to throw, then the mutex could be unset
    if (NULL == handle_) return;

    // This should always succeed.
    (void) CloseHandle(handle_);
  }

  void Lock() {
    DWORD ret;
    do {
      ret = WaitForSingleObject(handle_, INFINITE);
      if (WAIT_OBJECT_0 == ret) return;

      // If this lock wasn't abandoned or locked
      // the there must be an error.
      if (WAIT_ABANDONED != ret) {
        throw std::exception("Lock failed on mutex.");
      }
    } while (WAIT_ABANDONED == ret);
  }

  bool Try() {
    DWORD ret = WaitForSingleObject(handle_, 0);
    switch (ret) {
      case WAIT_OBJECT_0:
        return true;

      // The lock was already taken, we will need to retry.
      case WAIT_TIMEOUT:
        return false;

      // The lock was abandoned and ownership transfered, it is still unlocked
      // so we can just retry.
      case WAIT_ABANDONED:
        return false;

      // Unrecoverable error.
      default:
        throw std::exception("Try failed on mutex.");
    }
  }

  void Mutex::Unlock() {
    (void) ReleaseMutex(handle_);
  }

 private:
  HANDLE handle_;
};

IMutex* IMutex::Allocate() {
  return new Mutex;
}

void IMutex::Free(IMutex *mutex) {
  delete static_cast<Mutex*>(mutex);
}

}  // End of port namespace

