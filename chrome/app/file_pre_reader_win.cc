// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/file_pre_reader_win.h"

#include <windows.h>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/threading/thread_restrictions.h"

void PreReadFile(const base::FilePath& file_path) {
  base::ThreadRestrictions::AssertIOAllowed();

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

void PreReadMemoryMappedFile(const base::MemoryMappedFile& memory_mapped_file,
                             const base::FilePath& file_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (!memory_mapped_file.IsValid())
    return;

  // Load ::PrefetchVirtualMemory dynamically, because it is only available on
  // Win8+.
  using PrefetchVirtualMemoryPtr = decltype(::PrefetchVirtualMemory)*;
  PrefetchVirtualMemoryPtr prefetch_virtual_memory =
      reinterpret_cast<PrefetchVirtualMemoryPtr>(::GetProcAddress(
          ::GetModuleHandle(L"kernel32.dll"), "PrefetchVirtualMemory"));
  if (!prefetch_virtual_memory) {
    // If ::PrefetchVirtualMemory is not available, fall back to PreReadFile.
    PreReadFile(file_path);
    return;
  }

  WIN32_MEMORY_RANGE_ENTRY memory_range;
  memory_range.VirtualAddress = const_cast<void*>(
      reinterpret_cast<const void*>(memory_mapped_file.data()));
  memory_range.NumberOfBytes = memory_mapped_file.length();
  prefetch_virtual_memory(::GetCurrentProcess(), 1U, &memory_range, 0);
}
