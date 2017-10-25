// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILING_MEMLOG_SENDER_PIPE_POSIX_H_
#define CHROME_COMMON_PROFILING_MEMLOG_SENDER_PIPE_POSIX_H_

#include <string>
#include <vector>

#include "base/files/platform_file.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

namespace profiling {

class MemlogSenderPipe {
 public:
  explicit MemlogSenderPipe(base::ScopedPlatformFile file);
  ~MemlogSenderPipe();

  bool Connect();

  bool Send(const void* data, size_t sz);

 private:
  mojo::edk::ScopedPlatformHandle handle_;

  // All calls to Send() are wrapped in a Lock, since the size of the data might
  // be larger than the maximum atomic write size of a pipe on Posix [PIPE_BUF].
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(MemlogSenderPipe);
};

}  // namespace profiling

#endif  // CHROME_COMMON_PROFILING_MEMLOG_SENDER_PIPE_POSIX_H_
