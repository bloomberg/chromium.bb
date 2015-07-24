/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_NACL_ABI_DIRENT_H_
#define LIBRARIES_NACL_IO_NACL_ABI_DIRENT_H_

#include <dirent.h>

/* From native_client/src/trusted/service_runtime/include/sys/dirent.h */

#ifndef nacl_abi___ino_t_defined
#define nacl_abi___ino_t_defined
typedef int64_t nacl_abi___ino_t;
typedef nacl_abi___ino_t nacl_abi_ino_t;
#endif

#ifndef nacl_abi___off_t_defined
#define nacl_abi___off_t_defined
typedef int64_t nacl_abi__off_t;
typedef nacl_abi__off_t nacl_abi_off_t;
#endif

/* We need a way to define the maximum size of a name. */
#ifndef MAXNAMLEN
# ifdef NAME_MAX
#  define MAXNAMLEN NAME_MAX
# else
#  define MAXNAMLEN 255
# endif
#endif

struct nacl_abi_dirent {
  nacl_abi_ino_t nacl_abi_d_ino;
  nacl_abi_off_t nacl_abi_d_off;
  uint16_t       nacl_abi_d_reclen;
  char           nacl_abi_d_name[MAXNAMLEN + 1];
};

#endif  /* LIBRARIES_NACL_IO_NACL_ABI_DIRENT_H_ */
