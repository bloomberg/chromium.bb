// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/profiler.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"

namespace {
std::string GetProfileName() {
  static const char kDefaultProfileName[] = "chrome-profile-{pid}";
  static std::string profile_name;

  if (profile_name.empty()) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kProfilingFile))
      profile_name = command_line.GetSwitchValueASCII(switches::kProfilingFile);
    else
      profile_name = std::string(kDefaultProfileName);
    std::string process_type =
        command_line.GetSwitchValueASCII(switches::kProcessType);
    std::string type = process_type.empty() ?
        std::string("browser") : std::string(process_type);
    ReplaceSubstringsAfterOffset(&profile_name, 0, "{type}", type.c_str());
  }
  return profile_name;
}

void FlushProfilingData() {
  static const int kProfilingFlushSeconds = 10;

  if (!Profiling::BeingProfiled())
    return;

  base::debug::FlushProfiling();
  static int flush_seconds = 0;
  if (!flush_seconds) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    std::string profiling_flush =
        command_line.GetSwitchValueASCII(switches::kProfilingFlush);
    if (!profiling_flush.empty()) {
      flush_seconds = atoi(profiling_flush.c_str());
      DCHECK(flush_seconds > 0);
    } else {
      flush_seconds = kProfilingFlushSeconds;
    }
  }
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      NewRunnableFunction(FlushProfilingData),
      flush_seconds * 1000);
}

} // namespace

// static
void Profiling::ProcessStarted() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (command_line.HasSwitch(switches::kProfilingAtStart)) {
    std::string process_type_to_start =
        command_line.GetSwitchValueASCII(switches::kProfilingAtStart);
    if (process_type == process_type_to_start)
      Start();
  }
}

// static
void Profiling::Start() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool flush = command_line.HasSwitch(switches::kProfilingFlush);
  base::debug::StartProfiling(GetProfileName());

  // Schedule profile data flushing for single process because it doesn't
  // get written out correctly on exit.
  if (flush && MessageLoop::current())
    FlushProfilingData();
}

// static
void Profiling::Stop() {
  base::debug::StopProfiling();
}

// static
bool Profiling::BeingProfiled() {
  return base::debug::BeingProfiled();
}

// static
void Profiling::Toggle() {
  if (BeingProfiled())
    Stop();
  else
    Start();
}

// static
void Profiling::MainMessageLoopStarted() {
  if (BeingProfiled()) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    bool flush = command_line.HasSwitch(switches::kProfilingFlush);
    if (flush)
      FlushProfilingData();
  }
}
