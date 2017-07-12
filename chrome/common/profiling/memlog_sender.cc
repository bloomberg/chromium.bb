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

namespace {

// TODO(brettw) this is a hack to allow StartProfilingMojo to work. Figure out
// how to get the lifetime of this that allows that function call to work.
MemlogSenderPipe* memlog_sender_pipe = nullptr;

}  // namespace

void InitMemlogSenderIfNecessary(const base::CommandLine& cmdline) {
  std::string pipe_id = cmdline.GetSwitchValueASCII(switches::kMemlogPipe);
  if (!pipe_id.empty())
    StartMemlogSender(pipe_id);
}

void StartMemlogSender(const std::string& pipe_id) {
  static MemlogSenderPipe pipe(pipe_id);
  pipe.Connect();
  memlog_sender_pipe = &pipe;

  StreamHeader header;
  header.signature = kStreamSignature;
  pipe.Send(&header, sizeof(StreamHeader));

  InitAllocatorShim(&pipe);
}

void StartProfilingMojo() {
  static bool started_mojo = false;

  if (!started_mojo) {
    started_mojo = true;
    StartMojoControlPacket start_mojo_message;
    start_mojo_message.op = kStartMojoControlPacketType;
    memlog_sender_pipe->Send(&start_mojo_message, sizeof(start_mojo_message));
  }
}

}  // namespace profiling
