// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_NODE_TTY_H_
#define LIBRARIES_NACL_IO_MOUNT_NODE_TTY_H_

#include <pthread.h>

#include <deque>

#include "nacl_io/ioctl.h"
#include "nacl_io/mount_node_char.h"
#include "nacl_io/ostermios.h"


namespace nacl_io {

class MountNodeTty : public MountNodeCharDevice {
 public:
  explicit MountNodeTty(Mount* mount);
  ~MountNodeTty();

  virtual Error Ioctl(int request,
                      char* arg);

  virtual Error Read(size_t offs,
                     void* buf,
                     size_t count,
                     int* out_bytes);

  virtual Error Write(size_t offs,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

  virtual Error Tcgetattr(struct termios* termios_p);
  virtual Error Tcsetattr(int optional_actions,
                          const struct termios *termios_p);

 private:
  virtual Error Write(size_t offs,
                      const void* buf,
                      size_t count,
                      int* out_bytes,
                      bool locked);
  Error ProcessInput(struct tioc_nacl_input_string* message);
  Error Echo(const char* string, int count);
  void InitTermios();

  std::deque<char> input_buffer_;
  bool is_readable_;
  pthread_cond_t is_readable_cond_;
  std::string prefix_;
  struct termios termios_;
};

}

#endif
