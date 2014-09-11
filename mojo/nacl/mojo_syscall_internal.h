// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_NACL_MOJO_SYSCALL_INTERNAL_H_
#define MOJO_NACL_MOJO_SYSCALL_INTERNAL_H_

#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

namespace {

class ScopedCopyLock {
 public:
  explicit ScopedCopyLock(struct NaClApp* nap) : nap_(nap) {
    NaClCopyTakeLock(nap_);
  }
  ~ScopedCopyLock() {
    NaClCopyDropLock(nap_);
  }
 private:
  struct NaClApp* nap_;
};

static inline uintptr_t NaClUserToSysAddrArray(
    struct NaClApp* nap,
    uint32_t uaddr,
    size_t count,
    size_t size) {
  // TODO(ncbray): overflow checking
  size_t range = count * size;
  return NaClUserToSysAddrRange(nap, uaddr, range);
}

template <typename T> bool ConvertScalarInput(
    struct NaClApp* nap,
    uint32_t user_ptr,
    T* value) {
  if (user_ptr) {
    uintptr_t temp = NaClUserToSysAddrRange(nap, user_ptr, sizeof(T));
    if (temp != kNaClBadAddress) {
      *value = *reinterpret_cast<T volatile*>(temp);
      return true;
    }
  }
  return false;
}

template <typename T> bool ConvertScalarOutput(
    struct NaClApp* nap,
    uint32_t user_ptr,
    T volatile** sys_ptr) {
  if (user_ptr) {
    uintptr_t temp = NaClUserToSysAddrRange(nap, user_ptr, sizeof(T));
    if (temp != kNaClBadAddress) {
      *sys_ptr = reinterpret_cast<T volatile*>(temp);
      return true;
    }
  }
  *sys_ptr = 0; // Paranoia.
  return false;
}

template <typename T> bool ConvertScalarInOut(
    struct NaClApp* nap,
    uint32_t user_ptr,
    bool optional,
    T* value,
    T volatile** sys_ptr) {
  if (user_ptr) {
    uintptr_t temp = NaClUserToSysAddrRange(nap, user_ptr, sizeof(T));
    if (temp != kNaClBadAddress) {
      T volatile* converted = reinterpret_cast<T volatile*>(temp);
      *sys_ptr = converted;
      *value = *converted;
      return true;
    }
  } else if (optional) {
    *sys_ptr = 0;
    *value = static_cast<T>(0); // Paranoia.
    return true;
  }
  *sys_ptr = 0; // Paranoia.
  *value = static_cast<T>(0); // Paranoia.
  return false;
}

template <typename T> bool ConvertArray(
    struct NaClApp* nap,
    uint32_t user_ptr,
    uint32_t length,
    size_t element_size,
    bool optional,
    T** sys_ptr) {
  if (user_ptr) {
    uintptr_t temp = NaClUserToSysAddrArray(nap, user_ptr, length,
                                            element_size);
    if (temp != kNaClBadAddress) {
      *sys_ptr = reinterpret_cast<T*>(temp);
      return true;
    }
  } else if (optional) {
    *sys_ptr = 0;
    return true;
  }
  return false;
}

template <typename T> bool ConvertBytes(
    struct NaClApp* nap,
    uint32_t user_ptr,
    uint32_t length,
    bool optional,
    T** sys_ptr) {
  if (user_ptr) {
    uintptr_t temp = NaClUserToSysAddrRange(nap, user_ptr, length);
    if (temp != kNaClBadAddress) {
      *sys_ptr = reinterpret_cast<T*>(temp);
      return true;
    }
  } else if (optional) {
    *sys_ptr = 0;
    return true;
  }
  return false;
}

// TODO(ncbray): size validation and complete copy.
// TODO(ncbray): ensure non-null / missized structs are covered by a test case.
template <typename T> bool ConvertStruct(
    struct NaClApp* nap,
    uint32_t user_ptr,
    bool optional,
    T** sys_ptr) {
  if (user_ptr) {
    uintptr_t temp = NaClUserToSysAddrRange(nap, user_ptr, sizeof(T));
    if (temp != kNaClBadAddress) {
      *sys_ptr = reinterpret_cast<T*>(temp);
      return true;
    }
  } else if (optional) {
    *sys_ptr = 0;
    return true;
  }
  return false;
}

} // namespace

#endif // MOJO_NACL_MOJO_SYSCALL_INTERNAL_H_
