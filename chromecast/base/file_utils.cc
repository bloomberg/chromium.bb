// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/file_utils.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"

namespace {

// Calls flock on valid file descriptor |fd| with flag |flag|. Returns true
// on success, false on failure.
bool CallFlockOnFileWithFlag(const int fd, int flag) {
  int ret = -1;
  if ((ret = HANDLE_EINTR(flock(fd, flag))) < 0)
    PLOG(ERROR) << "Error locking " << fd;

  return !ret;
}

}  // namespace

namespace chromecast {

int OpenAndLockFile(const base::FilePath& path, bool write) {
  int fd = -1;
  const char* file = path.value().c_str();

  if ((fd = open(file, write ? O_RDWR : O_RDONLY)) < 0) {
    PLOG(ERROR) << "Error opening " << file;
  } else if (!CallFlockOnFileWithFlag(fd, LOCK_EX)) {
    close(fd);
    fd = -1;
  }

  return fd;
}

bool UnlockAndCloseFile(const int fd) {
  if (!CallFlockOnFileWithFlag(fd, LOCK_UN))
    return false;
  return !close(fd);
}

}  // namespace chromecast
