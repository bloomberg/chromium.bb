// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/profiler.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_switches.h"
#include "gin/public/debug.h"
#include "v8/include/v8.h"

namespace {

base::debug::AddDynamicSymbol add_dynamic_symbol_func = NULL;
base::debug::MoveDynamicSymbol move_dynamic_symbol_func = NULL;

void JitCodeEventHandler(const v8::JitCodeEvent* event) {
  DCHECK_NE(static_cast<base::debug::AddDynamicSymbol>(NULL),
            add_dynamic_symbol_func);
  DCHECK_NE(static_cast<base::debug::MoveDynamicSymbol>(NULL),
            move_dynamic_symbol_func);

  switch (event->type) {
    case v8::JitCodeEvent::CODE_ADDED:
      add_dynamic_symbol_func(event->code_start, event->code_len,
                              event->name.str, event->name.len);
      break;

    case v8::JitCodeEvent::CODE_MOVED:
      move_dynamic_symbol_func(event->code_start, event->new_code_start);
      break;

    default:
      break;
  }
}

std::string GetProfileName() {
  static const char kDefaultProfileName[] = "chrome-profile-{type}-{pid}";
  CR_DEFINE_STATIC_LOCAL(std::string, profile_name, ());

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

void FlushProfilingData(base::Thread* thread) {
  static const int kProfilingFlushSeconds = 10;

  if (!Profiling::BeingProfiled())
    return;

  base::debug::FlushProfiling();
  static int flush_seconds;
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
  thread->message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FlushProfilingData, thread),
      base::TimeDelta::FromSeconds(flush_seconds));
}

class ProfilingThreadControl {
 public:
  ProfilingThreadControl() : thread_(NULL) {}

  void Start() {
    base::AutoLock locked(lock_);

    if (thread_ && thread_->IsRunning())
      return;
    thread_ = new base::Thread("Profiling_Flush");
    thread_->Start();
    thread_->message_loop()->PostTask(
        FROM_HERE, base::Bind(&FlushProfilingData, thread_));
  }

  void Stop() {
    base::AutoLock locked(lock_);

    if (!thread_ || !thread_->IsRunning())
      return;
    thread_->Stop();
    delete thread_;
    thread_ = NULL;
  }

 private:
  base::Thread* thread_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ProfilingThreadControl);
};

base::LazyInstance<ProfilingThreadControl>::Leaky
    g_flush_thread_control = LAZY_INSTANCE_INITIALIZER;

} // namespace

// static
void Profiling::ProcessStarted() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  // Establish the V8 profiling hooks if we're an instrumented binary.
  if (base::debug::IsBinaryInstrumented()) {
    base::debug::ReturnAddressLocationResolver resolve_func =
        base::debug::GetProfilerReturnAddrResolutionFunc();

    if (resolve_func != NULL) {
      v8::V8::SetReturnAddressLocationResolver(resolve_func);
    }

    // Set up the JIT code entry handler and the symbol callbacks if the
    // profiler supplies them.
    // TODO(siggi): Maybe add a switch or an environment variable to turn off
    //     V8 profiling?
    base::debug::DynamicFunctionEntryHook entry_hook_func =
        base::debug::GetProfilerDynamicFunctionEntryHookFunc();
    add_dynamic_symbol_func = base::debug::GetProfilerAddDynamicSymbolFunc();
    move_dynamic_symbol_func = base::debug::GetProfilerMoveDynamicSymbolFunc();

    if (entry_hook_func != NULL &&
        add_dynamic_symbol_func != NULL &&
        move_dynamic_symbol_func != NULL) {
      gin::Debug::SetFunctionEntryHook(entry_hook_func);
      gin::Debug::SetJitCodeEventHandler(&JitCodeEventHandler);
    }
  }

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
  if (flush)
    g_flush_thread_control.Get().Start();
}

// static
void Profiling::Stop() {
  g_flush_thread_control.Get().Stop();
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
