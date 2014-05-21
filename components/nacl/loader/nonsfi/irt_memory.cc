// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <sys/mman.h>

#include "base/logging.h"
#include "components/nacl/loader/nonsfi/irt_interfaces.h"
#include "components/nacl/loader/nonsfi/irt_util.h"
#include "native_client/src/trusted/service_runtime/include/machine/_types.h"
#include "native_client/src/trusted/service_runtime/include/sys/mman.h"

namespace nacl {
namespace nonsfi {
namespace {

int NaClProtToProt(int nacl_prot) {
  int prot = 0;
  if ((nacl_prot & NACL_ABI_PROT_MASK) == NACL_ABI_PROT_NONE)
    return PROT_NONE;

  if (nacl_prot & NACL_ABI_PROT_READ)
    prot |= PROT_READ;
  if (nacl_prot & NACL_ABI_PROT_WRITE)
    prot |= PROT_WRITE;
  if (nacl_prot & NACL_ABI_PROT_EXEC)
    prot |= PROT_EXEC;
  return prot;
}

int NaClFlagsToFlags(int nacl_flags) {
  int flags = 0;

  if (nacl_flags & NACL_ABI_MAP_SHARED)
    flags |= MAP_SHARED;
  if (nacl_flags & NACL_ABI_MAP_PRIVATE)
    flags |= MAP_PRIVATE;
  if (nacl_flags & NACL_ABI_MAP_FIXED)
    flags |= MAP_FIXED;

  // Note: NACL_ABI_MAP_ANON is an alias of NACL_ABI_MAP_ANONYMOUS.
  if (nacl_flags & NACL_ABI_MAP_ANONYMOUS)
    flags |= MAP_ANONYMOUS;
  return flags;
}

int IrtMMap(void** addr, size_t len, int prot, int flags,
            int fd, nacl_abi_off_t off) {
  const int host_prot = NaClProtToProt(prot);
  // On Chrome OS, mmap can fail if PROT_EXEC is set in |host_prot|,
  // but mprotect will allow changing the permissions later.
  // This is because Chrome OS mounts writable filesystems with "noexec".
  void* result = mmap(
      *addr, len, host_prot & ~PROT_EXEC, NaClFlagsToFlags(flags), fd, off);
  if (result == MAP_FAILED)
    return errno;
  if (host_prot & PROT_EXEC) {
    if (mprotect(result, len, host_prot) != 0) {
      // This aborts here because it cannot easily undo the mmap() call.
      PLOG(FATAL) << "IrtMMap: mprotect to turn on PROT_EXEC failed.";
    }
  }

  *addr = result;
  return 0;
}

int IrtMUnmap(void* addr, size_t len) {
  return CheckError(munmap(addr, len));
}

int IrtMProtect(void* addr, size_t len, int prot) {
  return CheckError(mprotect(addr, len, NaClProtToProt(prot)));
}

}  // namespace

// For mmap, the argument types should be nacl_abi_off_t rather than off_t.
// However, the definition of nacl_irt_memory uses the host type off_t, so here
// we need to cast it.
const nacl_irt_memory kIrtMemory = {
  reinterpret_cast<int(*)(void**, size_t, int, int, int, off_t)>(IrtMMap),
  IrtMUnmap,
  IrtMProtect,
};

}  // namespace nonsfi
}  // namespace nacl
