// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nonsfi/irt_interfaces.h"

#include <cstring>

namespace nacl {
namespace nonsfi {

namespace {

// This table keeps a pair of IRT entry (such as nacl_irt_basic, nacl_irt_fdio
// etc.) and its registered name with its version (such as NACL_IRT_BASIC_v0_1,
// NACL_IRT_DEV_FDIO_v0_1, etc.).
struct NaClInterfaceTable {
  const char* name;
  const void* table;
  size_t size;
};

#define NACL_INTERFACE_TABLE(name, value) { name, &value, sizeof(value) }
const NaClInterfaceTable kIrtInterfaces[] = {
  NACL_INTERFACE_TABLE(NACL_IRT_BASIC_v0_1, kIrtBasic),
  NACL_INTERFACE_TABLE(NACL_IRT_FDIO_v0_1, kIrtFdIO),
  NACL_INTERFACE_TABLE(NACL_IRT_MEMORY_v0_3, kIrtMemory),
  NACL_INTERFACE_TABLE(NACL_IRT_THREAD_v0_1, kIrtThread),
  NACL_INTERFACE_TABLE(NACL_IRT_FUTEX_v0_1, kIrtFutex),
  NACL_INTERFACE_TABLE(NACL_IRT_TLS_v0_1, kIrtTls),
  NACL_INTERFACE_TABLE(NACL_IRT_CLOCK_v0_1, kIrtClock),
  NACL_INTERFACE_TABLE(NACL_IRT_PPAPIHOOK_v0_1, kIrtPpapiHook),
  NACL_INTERFACE_TABLE(NACL_IRT_RESOURCE_OPEN_v0_1, kIrtResourceOpen),
  NACL_INTERFACE_TABLE(NACL_IRT_RANDOM_v0_1, kIrtRandom),
  NACL_INTERFACE_TABLE(NACL_IRT_EXCEPTION_HANDLING_v0_1, kIrtExceptionHandling),
#if defined(__arm__)
  NACL_INTERFACE_TABLE(NACL_IRT_ICACHE_v0_1, kIrtIcache),
#endif
};
#undef NACL_INTERFACE_TABLE

}  // namespace

size_t NaClIrtInterface(const char* interface_ident,
                        void* table, size_t tablesize) {
  for (size_t i = 0; i < arraysize(kIrtInterfaces); ++i) {
    if (std::strcmp(interface_ident, kIrtInterfaces[i].name) == 0) {
      const size_t size = kIrtInterfaces[i].size;
      if (size <= tablesize) {
        std::memcpy(table, kIrtInterfaces[i].table, size);
        return size;
      }
      break;
    }
  }
  return 0;
}

}  // namespace nonsfi
}  // namespace nacl
