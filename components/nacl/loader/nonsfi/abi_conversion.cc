// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>

#include "base/logging.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/service_runtime/include/sys/time.h"

namespace nacl {
namespace nonsfi {

void NaClAbiTimeSpecToTimeSpec(const struct nacl_abi_timespec& nacl_timespec,
                               struct timespec* host_timespec) {
  host_timespec->tv_sec = nacl_timespec.tv_sec;
  host_timespec->tv_nsec = nacl_timespec.tv_nsec;
}

void TimeSpecToNaClAbiTimeSpec(const struct timespec& host_timespec,
                               struct nacl_abi_timespec* nacl_timespec) {
  nacl_timespec->tv_sec = host_timespec.tv_sec;
  nacl_timespec->tv_nsec = host_timespec.tv_nsec;
}

void StatToNaClAbiStat(
    const struct stat& host_stat, struct nacl_abi_stat* nacl_stat) {
  // Some fields in host_stat, such as st_dev, group/other bits of mode and
  // (a,m,c)timensec, are ignored to sync with the NaCl's original
  // implementation. Please see also NaClAbiStatHostDescStatXlateCtor in
  // native_client/src/trusted/desc/posix/nacl_desc.c.
  std::memset(nacl_stat, 0, sizeof(*nacl_stat));
  nacl_stat->nacl_abi_st_dev = 0;
  nacl_stat->nacl_abi_st_ino = host_stat.st_ino;
  nacl_abi_mode_t mode;
  switch (host_stat.st_mode & S_IFMT) {
    case S_IFREG:
      mode = NACL_ABI_S_IFREG;
      break;
    case S_IFDIR:
      mode = NACL_ABI_S_IFDIR;
      break;
    case S_IFCHR:
      mode = NACL_ABI_S_IFCHR;
      break;
    default:
      LOG(WARNING) << "Unusual NaCl descriptor type: "
                   << (host_stat.st_mode & S_IFMT);
      mode = NACL_ABI_S_UNSUP;
  }
  if (host_stat.st_mode & S_IRUSR)
    mode |= NACL_ABI_S_IRUSR;
  if (host_stat.st_mode & S_IWUSR)
    mode |= NACL_ABI_S_IWUSR;
  if (host_stat.st_mode & S_IXUSR)
    mode |= NACL_ABI_S_IXUSR;
  nacl_stat->nacl_abi_st_mode = mode;
  nacl_stat->nacl_abi_st_nlink = host_stat.st_nlink;
  nacl_stat->nacl_abi_st_uid = -1;  // Not root
  nacl_stat->nacl_abi_st_gid = -1;  // Not wheel
  nacl_stat->nacl_abi_st_rdev = 0;
  nacl_stat->nacl_abi_st_size = host_stat.st_size;
  nacl_stat->nacl_abi_st_blksize = 0;
  nacl_stat->nacl_abi_st_blocks = 0;
  nacl_stat->nacl_abi_st_atime = host_stat.st_atime;
  nacl_stat->nacl_abi_st_mtime = host_stat.st_mtime;
  nacl_stat->nacl_abi_st_ctime = host_stat.st_ctime;
}

}  // namespace nonsfi
}  // namespace nacl
