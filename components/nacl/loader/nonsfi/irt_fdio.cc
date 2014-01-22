// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "base/logging.h"
#include "components/nacl/loader/nonsfi/abi_conversion.h"
#include "components/nacl/loader/nonsfi/irt_interfaces.h"
#include "components/nacl/loader/nonsfi/irt_util.h"
#include "native_client/src/trusted/service_runtime/include/sys/dirent.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/service_runtime/include/sys/unistd.h"

namespace nacl {
namespace nonsfi {
namespace {

int IrtClose(int fd) {
  return CheckError(close(fd));
}

int IrtDup(int fd, int* newfd) {
  return CheckErrorWithResult(dup(fd), newfd);
}

int IrtDup2(int fd, int newfd) {
  return CheckError(dup2(fd, newfd));
}

int IrtRead(int fd, void* buf, size_t count, size_t* nread) {
  return CheckErrorWithResult(read(fd, buf, count), nread);
}

int IrtWrite(int fd, const void* buf, size_t count, size_t* nwrote) {
  return CheckErrorWithResult(write(fd, buf, count), nwrote);
}

int IrtSeek(int fd, nacl_abi_off_t offset, int whence,
            nacl_abi_off_t* new_offset) {
  return CheckErrorWithResult(lseek(fd, offset, whence), new_offset);
}

int IrtFstat(int fd, struct nacl_abi_stat* st) {
  struct stat host_st;
  if (fstat(fd, &host_st))
    return errno;

  StatToNaClAbiStat(host_st, st);
  return 0;
}

int IrtGetDents(int fd, struct nacl_abi_dirent* buf, size_t count,
                size_t* nread) {
  // Note: getdents() can return several directory entries in packed format.
  // So, here, because we need to convert the abi from host's to nacl's,
  // there is no straightforward way to ensure reading only entries which can
  // be fit to the buf after abi conversion actually.
  // TODO(https://code.google.com/p/nativeclient/issues/detail?id=3734):
  // Implement this method.
  return ENOSYS;
}

}  // namespace

// For seek, fstat and getdents, their argument types should be nacl_abi_X,
// rather than host type, such as off_t, struct stat or struct dirent.
// However, the definition of nacl_irt_fdio uses host types, so here we need
// to cast them.
const nacl_irt_fdio kIrtFdIO = {
  IrtClose,
  IrtDup,
  IrtDup2,
  IrtRead,
  IrtWrite,
  reinterpret_cast<int(*)(int, off_t, int, off_t*)>(IrtSeek),
  reinterpret_cast<int(*)(int, struct stat*)>(IrtFstat),
  reinterpret_cast<int(*)(int, struct dirent*, size_t, size_t*)>(IrtGetDents),
};

}  // namespace nonsfi
}  // namespace nacl
