// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_HANDLE_ENUMERATOR_WIN_H_
#define CONTENT_COMMON_HANDLE_ENUMERATOR_WIN_H_

#include "base/memory/ref_counted.h"
#include "base/process.h"
#include "base/string16.h"

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
  AlpcPortHandle,
  OtherHandle
};

static HandleType StringToHandleType(const string16& type);

static string16 GetAccessString(HandleType handle_type, ACCESS_MASK access);

class HandleEnumerator : public base::RefCountedThreadSafe<HandleEnumerator> {
 public:
  explicit HandleEnumerator(bool all_handles):
      all_handles_(all_handles) { }

  void EnumerateHandles();

 private:
  bool all_handles_;

  DISALLOW_COPY_AND_ASSIGN(HandleEnumerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_HANDLE_ENUMERATOR_WIN_H_

