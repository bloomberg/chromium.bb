/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_IOCTL_H_
#define LIBRARIES_NACL_IO_IOCTL_H_

#include <sys/types.h>

/*
 * ioctl to feed input to a tty node. Accepts a pointer to the following
 * struct (tioc_nacl_input_string), which contains a pointer to an array
 * of characters.
 */
#define TIOCNACLINPUT 0xadcd02

/*
 * ioctl to register an output handler with the tty node.  Will fail
 * with EALREADY if a handler is already registered.  Expects an
 * argument of type tioc_nacl_output.  The handler will be called during
 * calls to write() on the thread that calls write(), or, for echoed input
 * during the TIOCNACLINPUT ioctl() on the thread calling ioctl(). The
 * handler should return the number of bytes written/handled, or -errno
 * if an error occured.
 */
#define TIOCNACLOUTPUT 0xadcd03

/*
 * ioctl used to set a name for a JavaScript pipe.  The name
 * is a string that is used to uniquely identify messages posted to and from
 * JavaScript which signifies that the message is destined for a
 * particular pipe device.  For this reason each device must have a
 * unique prefix.  Until a prefix is set on a given pipe any I/O operations
 * will return EIO.
 */
#define TIOCNACLPIPENAME 0xadcd04

typedef char* tioc_nacl_jspipe_name;

struct tioc_nacl_input_string {
  size_t length;
  const char* buffer;
};


typedef ssize_t (*tioc_nacl_output_handler_t)(const char* buf,
                                              size_t count,
                                              void* user_data);

struct tioc_nacl_output {
  tioc_nacl_output_handler_t handler;
  void* user_data;
};


#endif  /* LIBRARIES_NACL_IO_NACL_IO_H_ */
