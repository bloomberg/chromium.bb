// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILING_MEMLOG_SENDER_PIPE_WIN_H_
#define CHROME_COMMON_PROFILING_MEMLOG_SENDER_PIPE_WIN_H_

#include <windows.h>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace profiling {

class MemlogSenderPipe {
 public:
  explicit MemlogSenderPipe(const base::string16& pipe_id);
  ~MemlogSenderPipe();

  bool Connect();

  bool Send(const void* data, size_t sz);

 private:
  base::string16 pipe_id_;

  HANDLE handle_;

  DISALLOW_COPY_AND_ASSIGN(MemlogSenderPipe);
};

}  // namespace profiling

#endif  // CHROME_COMMON_PROFILING_MEMLOG_SENDER_PIPE_WIN_H_
