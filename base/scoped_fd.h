// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(BASE_SCOPED_FD_H_) && defined(OS_POSIX)
#define BASE_SCOPED_FD_H_

#include "base/eintr_wrapper.h"

// POSIX only
//
// A wrapper class for file descriptors which automatically closes them when
// they go out of scope:
//   ScopedFd fd(open("/tmp/file", O_RDONLY));
//   read(fd.get(), ...);
class ScopedFd {
 public:
  ScopedFd()
      : fd_(-1) { }

  explicit ScopedFd(int fd)
      : fd_(fd) { }

  ~ScopedFd() {
    Close();
  }

  void Close() {
    if (fd_ >= 0) {
      HANDLE_EINTR(close(fd_));
      fd_ = -1;
    }
  }

  int get() const { return fd_; }

  int Take() {
    const int temp = fd_;
    fd_ = -1;
    return temp;
  }

  void Set(int new_fd) {
    Close();
    fd_ = new_fd;
  }

 private:
  int fd_;

  DISALLOW_EVIL_CONSTRUCTORS(ScopedFd);
};

#endif // BASE_SCOPED_FD_H_
