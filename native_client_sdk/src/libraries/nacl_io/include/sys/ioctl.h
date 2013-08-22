/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_INCLUDE_SYS_IOCTL_H_
#define LIBRARIES_NACL_IO_INCLUDE_SYS_IOCTL_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

int ioctl(int fd, unsigned long request, ...);

__END_DECLS

#endif  /* LIBRARIES_NACL_IO_INCLUDE_SYS_IOCTL_H_ */
