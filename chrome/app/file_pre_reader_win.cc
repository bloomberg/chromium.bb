// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/file_pre_reader_win.h"

#include <windows.h>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/windows_version.h"

namespace {

// A helper function to touch all pages in the range
// [base_addr, base_addr + length).
void TouchPagesInRange(const void* base_addr, size_t length) {
  DCHECK(base_addr);
  DCHECK_GT(length, static_cast<size_t>(0));

  // Get the system info so we know the page size. Also, make sure we use a
  // non-zero value for the page size; GetSystemInfo() is hookable/patchable,
  // and you never know what shenanigans someone could get up to.
  SYSTEM_INFO system_info = {};
  ::GetSystemInfo(&system_info);
  if (system_info.dwPageSize == 0)
    system_info.dwPageSize = 4096;

  // We don't want to read outside the byte range (which could trigger an
  // access violation), so let's figure out the exact locations of the first
  // and final bytes we want to read.
  volatile uint8_t const* touch_ptr =
      reinterpret_cast<uint8_t const*>(base_addr);
  volatile uint8_t const* final_touch_ptr = touch_ptr + length - 1;

  // Read the memory in the range [touch_ptr, final_touch_ptr] with a stride
  // of the system page size, to ensure that it's been paged in.
  uint8_t dummy;
  for (; touch_ptr < final_touch_ptr; touch_ptr += system_info.dwPageSize)
    dummy = *touch_ptr;
  dummy = *final_touch_ptr;
}

}  // namespace

bool PreReadFile(const base::FilePath& file_path, int step_size) {
  DCHECK_GT(step_size, 0);
  base::ThreadRestrictions::AssertIOAllowed();

  if (base::win::GetVersion() > base::win::VERSION_XP) {
    // Vista+ branch. On these OSes, the forced reads through the DLL actually
    // slows warm starts. The solution is to sequentially read file contents.
    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                   base::File::FLAG_SEQUENTIAL_SCAN);
    if (!file.IsValid())
      return false;

    char* buffer = reinterpret_cast<char*>(::VirtualAlloc(
        nullptr, static_cast<DWORD>(step_size), MEM_COMMIT, PAGE_READWRITE));
    if (!buffer)
      return false;

    while (file.ReadAtCurrentPos(buffer, step_size) > 0) {}

    ::VirtualFree(buffer, 0, MEM_RELEASE);
  } else {
    // WinXP branch. Here, reading the DLL from disk doesn't do what we want so
    // instead we pull the pages into memory and touch pages at a stride. We use
    // the system's page size as the stride, ignoring the passed in step_size,
    // to make sure each page in the range is touched.
    base::MemoryMappedFile file_memory_map;
    if (!file_memory_map.Initialize(file_path))
      return false;
    TouchPagesInRange(file_memory_map.data(), file_memory_map.length());
  }

  return true;
}
