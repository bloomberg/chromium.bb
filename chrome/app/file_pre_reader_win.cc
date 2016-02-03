// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/file_pre_reader_win.h"

#include <windows.h>
#include <stdint.h>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/pe_image.h"
#include "base/win/windows_version.h"

namespace {

// A helper function to touch all pages in the range
// [base_addr, base_addr + length).
void TouchPagesInRange(const void* base_addr, uint32_t length) {
  DCHECK(base_addr);
  DCHECK_GT(length, static_cast<uint32_t>(0));

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

void PreReadFile(const base::FilePath& file_path) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (base::win::GetVersion() > base::win::VERSION_XP) {
    // Vista+ branch. On these OSes, the forced reads through the file actually
    // slows warm starts. The solution is to sequentially read file contents.
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
  } else {
    // WinXP branch. Here, reading the DLL from disk doesn't do what we want so
    // instead we pull the pages into memory and touch pages at a stride. We use
    // the system's page size as the stride, to make sure each page in the range
    // is touched.

    // Don't show an error popup when |file_path| is not a valid PE file.
    UINT previous_error_mode = ::SetErrorMode(SEM_FAILCRITICALERRORS);
    ::SetErrorMode(previous_error_mode | SEM_FAILCRITICALERRORS);

    base::ScopedNativeLibrary dll_module(::LoadLibraryExW(
        file_path.value().c_str(), NULL,
        LOAD_WITH_ALTERED_SEARCH_PATH | DONT_RESOLVE_DLL_REFERENCES));

    ::SetErrorMode(previous_error_mode);

    // Pre-reading non-PE files is not supported on XP.
    if (!dll_module.is_valid())
      return;

    base::win::PEImage pe_image(dll_module.get());
    if (!pe_image.VerifyMagic())
      return;

    // We don't want to read past the end of the module (which could trigger
    // an access violation), so make sure to check the image size.
    PIMAGE_NT_HEADERS nt_headers = pe_image.GetNTHeaders();
    const uint32_t dll_module_length = nt_headers->OptionalHeader.SizeOfImage;

    // Page in the module.
    TouchPagesInRange(dll_module.get(), dll_module_length);
  }
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
