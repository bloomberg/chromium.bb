// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/multi_process_lock.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "base/eintr_wrapper.h"
#include "base/logging.h"

class MultiProcessLockLinux : public MultiProcessLock {
 public:
  explicit MultiProcessLockLinux(const std::string& name)
      : name_(name), fd_(-1) { }

  virtual ~MultiProcessLockLinux() {
    if (fd_ != -1) {
      Unlock();
    }
  }

  virtual bool TryLock() {
    if (fd_ != -1) {
      DLOG(ERROR) << "MultiProcessLock is already locked - " << name_;
      return true;
    }

    if (name_.length() > MULTI_PROCESS_LOCK_NAME_MAX_LEN) {
      LOG(ERROR) << "Socket name too long (" << name_.length()
                 << " > " << MULTI_PROCESS_LOCK_NAME_MAX_LEN << ") - " << name_;
      return false;
    }

    struct sockaddr_un address;

    // +1 for terminator, +1 for 0 in position 0 that makes it an
    // abstract named socket.
    // If this assert fails it is because sockaddr_un.sun_path size has been
    // redefined and MULTI_PROCESS_LOCK_NAME_MAX_LEN can change accordingly.
    COMPILE_ASSERT(sizeof(address.sun_path)
        == MULTI_PROCESS_LOCK_NAME_MAX_LEN + 2, sun_path_size_changed);

    memset(&address, 0, sizeof(address));
    int print_length = snprintf(&address.sun_path[1],
                                MULTI_PROCESS_LOCK_NAME_MAX_LEN + 1,
                                "%s", name_.c_str());

    if (print_length < 0 ||
        print_length > static_cast<int>(MULTI_PROCESS_LOCK_NAME_MAX_LEN)) {
      PLOG(ERROR) << "Couldn't create sun_path - " << name_;
      return false;
    }

    // Must set the first character of the path to something non-zero
    // before we call SUN_LEN which depends on strcpy working.
    address.sun_path[0] = '@';
    size_t length = SUN_LEN(&address);

    // Reset the first character of the path back to zero so that
    // bind returns an abstract name socket.
    address.sun_path[0] = 0;
    address.sun_family = AF_LOCAL;

    int socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (socket_fd < 0) {
      PLOG(ERROR) << "Couldn't create socket - " << name_;
      return false;
    }

    if (bind(socket_fd,
             reinterpret_cast<sockaddr *>(&address),
             length) == 0) {
      fd_ = socket_fd;
      return true;
    } else {
      PLOG(ERROR) << "Couldn't bind socket - "
                  << &(address.sun_path[1])
                  << " Length: " << length;
      if (HANDLE_EINTR(close(socket_fd)) < 0) {
        PLOG(ERROR) << "close";
      }
      return false;
    }
  }

  virtual void Unlock() {
    if (fd_ == -1) {
      DLOG(ERROR) << "Over-unlocked MultiProcessLock - " << name_;
      return;
    }
    if (HANDLE_EINTR(close(fd_)) < 0) {
      PLOG(ERROR) << "close";
    }
    fd_ = -1;
  }

 private:
  std::string name_;
  int fd_;
  DISALLOW_COPY_AND_ASSIGN(MultiProcessLockLinux);
};

MultiProcessLock* MultiProcessLock::Create(const std::string &name) {
  return new MultiProcessLockLinux(name);
}
