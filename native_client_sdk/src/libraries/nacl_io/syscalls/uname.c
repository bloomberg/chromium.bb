/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

int uname(struct utsname* buf) {
  memset(buf, 0, sizeof(struct utsname));
  snprintf(buf->sysname, _UTSNAME_LENGTH, "NaCl");
  /* TODO(sbc): Fill out the other fields with useful information. */
  return 0;
}
