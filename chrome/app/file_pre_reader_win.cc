// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/file_pre_reader_win.h"

#include <windows.h>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "components/startup_metric_utils/common/pre_read_field_trial_utils_win.h"

namespace {

// Pre-reads |file_path| using ::ReadFile.
void PreReadFileUsingReadFile(const base::FilePath& file_path) {
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                 base::File::FLAG_SEQUENTIAL_SCAN);
  if (!file.IsValid())
    return;

  const DWORD kStepSize = 1024 * 1024;
  char* buffer = reinterpret_cast<char*>(
      ::VirtualAlloc(nullptr, kStepSize, MEM_COMMIT, PAGE_READWRITE));
  if (!buffer)
    return;

  while (file.ReadAtCurrentPos(buffer, kStepSize) > 0) {}

  ::VirtualFree(buffer, 0, MEM_RELEASE);
}

// Pre-reads |file_path| using ::PrefetchVirtualMemory, if available. Otherwise,
// falls back on using ::ReadFile.
void PreReadFileUsingPrefetchVirtualMemory(const base::FilePath& file_path) {
  // Load ::PrefetchVirtualMemory dynamically, because it is only available on
  // Win8+.
  using PrefetchVirtualMemoryPtr = decltype(::PrefetchVirtualMemory)*;
  PrefetchVirtualMemoryPtr prefetch_virtual_memory =
      reinterpret_cast<PrefetchVirtualMemoryPtr>(::GetProcAddress(
          ::GetModuleHandle(L"kernel32.dll"), "PrefetchVirtualMemory"));
  if (!prefetch_virtual_memory) {
    // If ::PrefetchVirtualMemory is not available, fall back to
    // PreReadFileUsingReadFile().
    PreReadFileUsingReadFile(file_path);
    return;
  }

  base::MemoryMappedFile memory_mapped_file;
  if (!memory_mapped_file.Initialize(file_path)) {
    // Initializing the memory map should not fail. If it does fail in a debug
    // build, we want to be warned about it so that we can investigate the
    // failure.
    NOTREACHED();
    PreReadFileUsingReadFile(file_path);
    return;
  }

  WIN32_MEMORY_RANGE_ENTRY memory_range;
  memory_range.VirtualAddress = const_cast<void*>(
      reinterpret_cast<const void*>(memory_mapped_file.data()));
  memory_range.NumberOfBytes = memory_mapped_file.length();
  prefetch_virtual_memory(::GetCurrentProcess(), 1U, &memory_range, 0);
}

}  // namespace

void PreReadFile(const base::FilePath& file_path,
                 const startup_metric_utils::PreReadOptions& pre_read_options) {
  DCHECK(pre_read_options.pre_read);
  base::ThreadRestrictions::AssertIOAllowed();

  // Increase thread priority if necessary.
  base::ThreadPriority previous_priority = base::ThreadPriority::NORMAL;
  if (pre_read_options.high_priority) {
    previous_priority = base::PlatformThread::GetCurrentThreadPriority();
    base::PlatformThread::SetCurrentThreadPriority(
        base::ThreadPriority::DISPLAY);
  }

  // Pre-read |file_path|.
  if (pre_read_options.prefetch_virtual_memory)
    PreReadFileUsingPrefetchVirtualMemory(file_path);
  else
    PreReadFileUsingReadFile(file_path);

  // Reset thread priority.
  if (pre_read_options.high_priority)
    base::PlatformThread::SetCurrentThreadPriority(previous_priority);
}
