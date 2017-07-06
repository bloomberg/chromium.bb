// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILING_MEMLOG_SENDER_PIPE_POSIX_H_
#define CHROME_COMMON_PROFILING_MEMLOG_SENDER_PIPE_POSIX_H_

#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"

namespace profiling {

class MemlogSenderPipe {
 public:
  explicit MemlogSenderPipe(const std::string& pipe_id);
  ~MemlogSenderPipe();

  bool Connect();

  bool Send(const void* data, size_t sz);

 private:
  base::ScopedFD fd_;

  // Make base::UnixDomainSocket::SendMsg happy.
  // TODO(ajwong): This is not really threadsafe. Fix.
  std::vector<int>* dummy_for_send_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MemlogSenderPipe);
};

}  // namespace profiling

#endif  // CHROME_COMMON_PROFILING_MEMLOG_SENDER_PIPE_POSIX_H_
