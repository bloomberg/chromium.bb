// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>

#include "base/logging.h"
#include "base/process/kill.h"

namespace base {

Process::Process(ProcessHandle handle) : process_(handle) {
  CHECK_NE(handle, GetCurrentProcessHandle());
}

Process::Process(RValue other)
    : process_(other.object->process_) {
  other.object->Close();
}

Process& Process::operator=(RValue other) {
  if (this != other.object) {
    process_ = other.object->process_;
    other.object->Close();
  }
  return *this;
}

// static
Process Process::Current() {
  Process process;
  process.process_ = GetCurrentProcessHandle();
  return process.Pass();
}

// static
Process Process::DeprecatedGetProcessFromHandle(ProcessHandle handle) {
  DCHECK_NE(handle, GetCurrentProcessHandle());
  return Process(handle);
}

#if !defined(OS_LINUX)
// static
bool Process::CanBackgroundProcesses() {
  return false;
}
#endif  // !defined(OS_LINUX)

bool Process::IsValid() const {
  return process_ != kNullProcessHandle;
}

ProcessHandle Process::Handle() const {
  return process_;
}

Process Process::Duplicate() const {
  if (is_current())
    return Current();

  return Process(process_);
}

ProcessId Process::pid() const {
  DCHECK(IsValid());
  return GetProcId(process_);
}

bool Process::is_current() const {
  return process_ == GetCurrentProcessHandle();
}

void Process::Close() {
  process_ = kNullProcessHandle;
  // if the process wasn't terminated (so we waited) or the state
  // wasn't already collected w/ a wait from process_utils, we're gonna
  // end up w/ a zombie when it does finally exit.
}

void Process::Terminate(int result_code) {
  // result_code isn't supportable.
  DCHECK(IsValid());
  // We don't wait here. It's the responsibility of other code to reap the
  // child.
  KillProcess(process_, result_code, false);
}

#if !defined(OS_LINUX)
bool Process::IsProcessBackgrounded() const {
  // See SetProcessBackgrounded().
  DCHECK(IsValid());
  return false;
}

bool Process::SetProcessBackgrounded(bool value) {
  // POSIX only allows lowering the priority of a process, so if we
  // were to lower it we wouldn't be able to raise it back to its initial
  // priority.
  DCHECK(IsValid());
  return false;
}
#endif  // !defined(OS_LINUX)

int Process::GetPriority() const {
  DCHECK(IsValid());
  return getpriority(PRIO_PROCESS, process_);
}

}  // namspace base
