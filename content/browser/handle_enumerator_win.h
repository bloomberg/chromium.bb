// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HANDLE_ENUMERATOR_WIN_H_
#define CONTENT_BROWSER_HANDLE_ENUMERATOR_WIN_H_

#include "base/memory/ref_counted.h"
#include "base/process.h"
#include "content/common/child_process_info.h"

namespace content {

enum HandleType {
  ProcessHandle,
  ThreadHandle,
  FileHandle,
  DirectoryHandle,
  KeyHandle,
  WindowStationHandle,
  DesktopHandle,
  ServiceHandle,
  EventHandle,
  MutexHandle,
  SemaphoreHandle,
  TimerHandle,
  NamedPipeHandle,
  JobHandle,
  FileMapHandle,
  OtherHandle
};

static HandleType StringToHandleType(const string16& type);

static string16 ProcessTypeString(ChildProcessInfo::ProcessType process_type);

static string16 GetAccessString(HandleType handle_type, ACCESS_MASK access);

class HandleEnumerator : public base::RefCountedThreadSafe<HandleEnumerator> {
 public:
  enum HandleEnumStatus {
    Starting,
    InProgress,
    Finished
  };

  HandleEnumerator(base::ProcessHandle handle, bool all_handles):
      handle_(handle),
      type_(ChildProcessInfo::UNKNOWN_PROCESS),
      status_(Starting),
      all_handles_(all_handles) { }

  ChildProcessInfo::ProcessType type() const { return type_; }

  HandleEnumStatus status() const { return status_; }

  void RunHandleEnumeration();

  void EnumerateHandles();

 private:
  void FindProcessOnIOThread();

  void FindProcessOnUIThread();

  void EnumerateHandlesAndTerminateProcess();

  base::ProcessHandle handle_;
  ChildProcessInfo::ProcessType type_;
  HandleEnumStatus status_;
  bool all_handles_;

  DISALLOW_COPY_AND_ASSIGN(HandleEnumerator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_HANDLE_ENUMERATOR_WIN_H_
