// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling/memlog_sender.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiling/memlog_allocator_shim.h"
#include "chrome/common/profiling/memlog_sender_pipe.h"
#include "chrome/common/profiling/memlog_stream.h"

namespace profiling {

void InitMemlogSenderIfNecessary(const base::CommandLine& cmdline) {
  base::CommandLine::StringType pipe_id =
      cmdline.GetSwitchValueNative(switches::kMemlogPipe);
  if (pipe_id.empty())
    return;  // No pipe, don't run.

  static MemlogSenderPipe pipe(pipe_id);
  pipe.Connect();

  StreamHeader header;
  header.signature = kStreamSignature;

  pipe.Send(&header, sizeof(StreamHeader));

  InitAllocatorShim(&pipe);
}

}  // namespace profiling
