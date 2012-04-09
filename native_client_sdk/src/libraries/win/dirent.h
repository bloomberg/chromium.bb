/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_WIN_DIRENT_H_
#define LIBRARIES_WIN_DIRENT_H_

#include <stdint.h>
#include "macros.h"

EXTERN_C_BEGIN

/**
* Implementation of dirent.h for building the SDK natively Windows.
*/
struct dirent {
  ino_t d_ino;
  off_t d_off;
  uint16_t d_reclen;
  char d_name[256];
};

EXTERN_C_END

#endif  /* LIBRARIES_WIN_DIRENT_H_ */
