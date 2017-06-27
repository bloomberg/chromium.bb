// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_SERVER_H_
#define CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_SERVER_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include "memlog_receiver_pipe_server_win.h"
#else
#include "memlog_receiver_pipe_server_posix.h"
#endif

#endif  // CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_SERVER_H_
