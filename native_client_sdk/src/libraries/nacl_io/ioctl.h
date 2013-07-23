/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_IOCTL_H_
#define LIBRARIES_NACL_IO_IOCTL_H_

/* ioctl to tell a tty mount to prefix every message with a particular
 * null-terminated string. Accepts a pointer to a C string which will
 * be the prefix.
 */
#define TIOCNACLPREFIX 0xadcd01

/* ioctl to feed input to a tty mount. Accepts a pointer to the following
 * struct (tioc_nacl_input_string), which contains a pointer to an array
 * of characters.
 */
#define TIOCNACLINPUT  0xadcd02

struct tioc_nacl_input_string {
  size_t length;
  const char* buffer;
};

#endif  /* LIBRARIES_NACL_IO_NACL_IO_H_ */
