// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILING_MEMLOG_SENDER_PIPE_H_
#define CHROME_COMMON_PROFILING_MEMLOG_SENDER_PIPE_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include "chrome/common/profiling/memlog_sender_pipe_win.h"
#else
#include "chrome/common/profiling/memlog_sender_pipe_posix.h"
#endif

#endif  // CHROME_COMMON_PROFILING_MEMLOG_SENDER_PIPE_H_
