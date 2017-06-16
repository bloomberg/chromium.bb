// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILING_MEMLOG_STREAM_H_
#define CHROME_COMMON_PROFILING_MEMLOG_STREAM_H_

#include "build/build_config.h"

namespace profiling {

#if defined(OS_WIN)
// Prefix for pipe name for communicating between chrome processes and the
// memlog process. The pipe ID is appended to this to get the pipe name.
extern const wchar_t kWindowsPipePrefix[];
#endif  // OS_WIN

}  // namespace profiling

#endif  // CHROME_COMMON_PROFILING_MEMLOG_STREAM_H_
