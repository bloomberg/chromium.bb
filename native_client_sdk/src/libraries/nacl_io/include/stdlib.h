/* Copyright (c) 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_INCLUDE_STDLIB_H_
#define LIBRARIES_NACL_IO_INCLUDE_STDLIB_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

char* realpath(const char* path, char* resolved_path);

__END_DECLS

#endif

#include_next <stdlib.h>
