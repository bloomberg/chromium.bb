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
#include "native_client/src/trusted/service_runtime/include/sys/dirent.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/service_runtime/include/sys/unistd.h"

namespace nacl {
namespace nonsfi {
namespace {

int IrtClose(int fd) {
  if (close(fd))
    return errno;

  return 0;
}

int IrtDup(int fd, int* newfd) {
  int result = dup(fd);
  if (result < 0)
    return errno;

  *newfd = result;
  return 0;
}

int IrtDup2(int fd, int newfd) {
  if (dup2(fd, newfd) < 0)
    return errno;

  return 0;
}

int IrtRead(int fd, void* buf, size_t count, size_t* nread) {
  ssize_t result = read(fd, buf, count);
  if (result < 0)
    return errno;

  *nread = result;
  return 0;
}

int IrtWrite(int fd, const void* buf, size_t count, size_t* nwrote) {
  ssize_t result = write(fd, buf, count);
  if (result < 0)
    return errno;

  *nwrote = result;
  return 0;
}

int IrtSeek(int fd, nacl_abi_off_t offset, int whence,
            nacl_abi_off_t* new_offset) {
  off_t result = lseek(fd, offset, whence);
  if (result < 0)
    return errno;

  *new_offset = result;
  return 0;
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
