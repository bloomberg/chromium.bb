// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/windows_version.h"

namespace base {

Process::Process(ProcessHandle handle)
    : is_current_process_(false),
      process_(handle) {
  CHECK_NE(handle, ::GetCurrentProcess());
}

Process::Process(RValue other)
    : is_current_process_(other.object->is_current_process_),
      process_(other.object->process_.Take()) {
  other.object->Close();
}

Process& Process::operator=(RValue other) {
  if (this != other.object) {
    process_.Set(other.object->process_.Take());
    is_current_process_ = other.object->is_current_process_;
    other.object->Close();
  }
  return *this;
}

// static
Process Process::Current() {
  Process process;
  process.is_current_process_ = true;
  return process.Pass();
}

// static
Process Process::DeprecatedGetProcessFromHandle(ProcessHandle handle) {
  DCHECK_NE(handle, ::GetCurrentProcess());
  ProcessHandle out_handle;
  if (!::DuplicateHandle(GetCurrentProcess(), handle,
                         GetCurrentProcess(), &out_handle,
                         0, FALSE, DUPLICATE_SAME_ACCESS)) {
    return Process();
  }
  return Process(out_handle);
}

// static
bool Process::CanBackgroundProcesses() {
  return true;
}

bool Process::IsValid() const {
  return process_.IsValid() || is_current();
}

ProcessHandle Process::Handle() const {
  return is_current_process_ ? GetCurrentProcess() : process_.Get();
}

Process Process::Duplicate() const {
  if (is_current())
    return Current();

  ProcessHandle out_handle;
  if (!IsValid() || !::DuplicateHandle(GetCurrentProcess(),
                                       Handle(),
                                       GetCurrentProcess(),
                                       &out_handle,
                                       0,
                                       FALSE,
                                       DUPLICATE_SAME_ACCESS)) {
    return Process();
  }
  return Process(out_handle);
}

ProcessId Process::pid() const {
  DCHECK(IsValid());
  return GetProcId(Handle());
}

bool Process::is_current() const {
  return is_current_process_;
}

void Process::Close() {
  is_current_process_ = false;
  if (!process_.IsValid())
    return;

  process_.Close();
}

void Process::Terminate(int result_code) {
  DCHECK(IsValid());

  // Call NtTerminateProcess directly, without going through the import table,
  // which might have been hooked with a buggy replacement by third party
  // software. http://crbug.com/81449.
  HMODULE module = GetModuleHandle(L"ntdll.dll");
  typedef UINT (WINAPI *TerminateProcessPtr)(HANDLE handle, UINT code);
  TerminateProcessPtr terminate_process = reinterpret_cast<TerminateProcessPtr>(
      GetProcAddress(module, "NtTerminateProcess"));
  terminate_process(Handle(), result_code);
}

bool Process::IsProcessBackgrounded() const {
  DCHECK(IsValid());
  DWORD priority = GetPriority();
  if (priority == 0)
    return false;  // Failure case.
  return ((priority == BELOW_NORMAL_PRIORITY_CLASS) ||
          (priority == IDLE_PRIORITY_CLASS));
}

bool Process::SetProcessBackgrounded(bool value) {
  DCHECK(IsValid());
  // Vista and above introduce a real background mode, which not only
  // sets the priority class on the threads but also on the IO generated
  // by it. Unfortunately it can only be set for the calling process.
  DWORD priority;
  if ((base::win::GetVersion() >= base::win::VERSION_VISTA) && (is_current())) {
    priority = value ? PROCESS_MODE_BACKGROUND_BEGIN :
                       PROCESS_MODE_BACKGROUND_END;
  } else {
    priority = value ? BELOW_NORMAL_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS;
  }

  return (::SetPriorityClass(Handle(), priority) != 0);
}

int Process::GetPriority() const {
  DCHECK(IsValid());
  return ::GetPriorityClass(Handle());
}

}  // namespace base
