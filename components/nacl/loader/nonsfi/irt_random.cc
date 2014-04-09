// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <unistd.h>

#include "base/logging.h"
#include "components/nacl/loader/nonsfi/irt_interfaces.h"
#include "components/nacl/loader/nonsfi/irt_util.h"

namespace nacl {
namespace nonsfi {
namespace {

// FD for urandom.
int secure_random_fd = -1;

int IrtGetRandomBytes(void* buf, size_t count, size_t* nread) {
  DCHECK_NE(secure_random_fd, -1);
  return CheckErrorWithResult(read(secure_random_fd, buf, count),
                              nread);
}

}  // namespace

const nacl_irt_random kIrtRandom = {
  IrtGetRandomBytes
};

void SetUrandomFd(int fd) {
  DCHECK_EQ(secure_random_fd, -1);
  secure_random_fd = fd;
}

}  // namespace nonsfi
}  // namespace nacl
