// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling/memlog_sender.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiling/memlog_allocator_shim.h"
#include "chrome/common/profiling/memlog_sender_pipe.h"
#include "chrome/common/profiling/memlog_stream.h"

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/profiling/profiling_constants.h"
#include "content/public/common/content_switches.h"
#endif

namespace profiling {

namespace {

// TODO(brettw) this is a hack to allow StartProfilingMojo to work. Figure out
// how to get the lifetime of this that allows that function call to work.
MemlogSenderPipe* memlog_sender_pipe = nullptr;

}  // namespace

void InitMemlogSenderIfNecessary() {
  const base::CommandLine& cmdline = *base::CommandLine::ForCurrentProcess();
  std::string pipe_id = cmdline.GetSwitchValueASCII(switches::kMemlogPipe);
  if (!pipe_id.empty()) {
#if defined(OS_WIN)
    StartMemlogSender(pipe_id);
#else
    // TODO(ajwong): The posix value of the kMemlogPipe is bogus. Fix? This
    // might be true for windows too if everything is done via the launch
    // handles as opposed to the original code's use of a shared pipe name.
    //
    // TODO(ajwong): This still does not work on Mac because the hook to insert
    // the file mapping is linux only.

    // TODO(ajwong): In posix, the startup sequence does not correctly pass the
    // file handle down to the gpu process still. Abort if it's a gpu process
    // for now.
    if (cmdline.GetSwitchValueASCII(switches::kProcessType) ==
        switches::kGpuProcess) {
      return;
    }

    base::ScopedFD fd(
        base::GlobalDescriptors::GetInstance()->Get(kProfilingDataPipe));
    // TODO(ajwong): Change the StartMemlogSender to take a Scoped-somthing
    // instead of a string to make lifetimes more clear.
    StartMemlogSender(base::IntToString(fd.release()));
#endif
  }
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
