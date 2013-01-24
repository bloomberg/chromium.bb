// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_process.h"

#if defined(OS_POSIX) && !defined(OS_ANDROID)
#include <signal.h>  // For SigUSR1Handler below.
#endif

#include "base/message_loop.h"
#include "base/metrics/statistics_recorder.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "content/common/child_thread.h"

#if defined(OS_ANDROID)
#include "base/debug/debugger.h"
// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
#include <sys/resource.h>
#endif

#if defined(OS_POSIX) && !defined(OS_ANDROID)
static void SigUSR1Handler(int signal) { }
#endif

namespace content {

ChildProcess* ChildProcess::child_process_;

#if defined(OS_ANDROID)
// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
namespace {
void SetHighThreadPriority() {
  int nice_value = -6; // High priority.
  setpriority(PRIO_PROCESS, base::PlatformThread::CurrentId(), nice_value);
}
}
#endif

ChildProcess::ChildProcess()
    : ref_count_(0),
      shutdown_event_(true, false),
      io_thread_("Chrome_ChildIOThread") {
  DCHECK(!child_process_);
  child_process_ = this;

  base::StatisticsRecorder::Initialize();

  // We can't recover from failing to start the IO thread.
  CHECK(io_thread_.StartWithOptions(
            base::Thread::Options(MessageLoop::TYPE_IO, 0)));

#if defined(OS_ANDROID)
  // TODO(epenner): Move thread priorities to base. (crbug.com/170549)
  io_thread_.message_loop()->PostTask(FROM_HERE,
                                      base::Bind(&SetHighThreadPriority));
#endif
}

ChildProcess::~ChildProcess() {
  DCHECK(child_process_ == this);

  // Signal this event before destroying the child process.  That way all
  // background threads can cleanup.
  // For example, in the renderer the RenderThread instances will be able to
  // notice shutdown before the render process begins waiting for them to exit.
  shutdown_event_.Signal();

  // Kill the main thread object before nulling child_process_, since
  // destruction code might depend on it.
  main_thread_.reset();

  child_process_ = NULL;
}

ChildThread* ChildProcess::main_thread() {
  return main_thread_.get();
}

void ChildProcess::set_main_thread(ChildThread* thread) {
  main_thread_.reset(thread);
}

void ChildProcess::AddRefProcess() {
  DCHECK(!main_thread_.get() ||  // null in unittests.
         MessageLoop::current() == main_thread_->message_loop());
  ref_count_++;
}

void ChildProcess::ReleaseProcess() {
  DCHECK(!main_thread_.get() ||  // null in unittests.
         MessageLoop::current() == main_thread_->message_loop());
  DCHECK(ref_count_);
  DCHECK(child_process_);
  if (--ref_count_)
    return;

  if (main_thread_.get())  // null in unittests.
    main_thread_->OnProcessFinalRelease();
}

base::WaitableEvent* ChildProcess::GetShutDownEvent() {
  DCHECK(child_process_);
  return &child_process_->shutdown_event_;
}

void ChildProcess::WaitForDebugger(const std::string& label) {
#if defined(OS_WIN)
#if defined(GOOGLE_CHROME_BUILD)
  std::string title = "Google Chrome";
#else  // CHROMIUM_BUILD
  std::string title = "Chromium";
#endif  // CHROMIUM_BUILD
  title += " ";
  title += label;  // makes attaching to process easier
  std::string message = label;
  message += " starting with pid: ";
  message += base::IntToString(base::GetCurrentProcId());
  ::MessageBox(NULL, UTF8ToWide(message).c_str(), UTF8ToWide(title).c_str(),
               MB_OK | MB_SETFOREGROUND);
#elif defined(OS_POSIX)
#if defined(OS_ANDROID)
  LOG(ERROR) << label << " waiting for GDB.";
  // Wait 24 hours for a debugger to be attached to the current process.
  base::debug::WaitForDebugger(24 * 60 * 60, false);
#else
  // TODO(playmobil): In the long term, overriding this flag doesn't seem
  // right, either use our own flag or open a dialog we can use.
  // This is just to ease debugging in the interim.
  LOG(ERROR) << label
             << " ("
             << getpid()
             << ") paused waiting for debugger to attach @ pid";
  // Install a signal handler so that pause can be woken.
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SigUSR1Handler;
  sigaction(SIGUSR1, &sa, NULL);

  pause();
#endif  // defined(OS_ANDROID)
#endif  // defined(OS_POSIX)
}

}  // namespace content
