// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/image_pre_reader_win.h"

#include <windows.h>
#include <algorithm>
#include <limits>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/pe_image.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"

namespace {

// The minimum buffer size to allocate when reading the PE file headers.
//
// The PE file headers usually fit into a single 1KB page, and a PE file must
// at least contain the initial page with the headers. That said, as long as
// we expect at least sizeof(IMAGE_DOS_HEADER) bytes, we're ok.
const size_t kMinHeaderBufferSize = 0x400;

// A handy symbolic constant.
const size_t kOneHundredPercent = 100;

void StaticAssertions() {
  COMPILE_ASSERT(kMinHeaderBufferSize >= sizeof(IMAGE_DOS_HEADER),
                 min_header_buffer_size_at_least_as_big_as_the_dos_header);
}

// This struct provides a deallocation functor for use with scoped_ptr<T>
// allocated with ::VirtualAlloc().
struct VirtualFreeDeleter {
  void operator() (void* ptr) {
    ::VirtualFree(ptr, 0, MEM_RELEASE);
  }
};

// A wrapper for the Win32 ::SetFilePointer() function with some error checking.
bool SetFilePointer(HANDLE file_handle, size_t position) {
  return position <= static_cast<size_t>(std::numeric_limits<LONG>::max()) &&
      ::SetFilePointer(file_handle,
                       static_cast<LONG>(position),
                       NULL,
                       FILE_BEGIN) != INVALID_SET_FILE_POINTER;
}

// A helper function to read the next |bytes_to_read| bytes from the file
// given by |file_handle| into |buffer|.
bool ReadNextBytes(HANDLE file_handle, void* buffer, size_t bytes_to_read) {
  DCHECK(file_handle != INVALID_HANDLE_VALUE);
  DCHECK(buffer != NULL);
  DCHECK(bytes_to_read > 0);

  DWORD bytes_read = 0;
  return bytes_to_read <= std::numeric_limits<DWORD>::max() &&
      ::ReadFile(file_handle,
                 buffer,
                 static_cast<DWORD>(bytes_to_read),
                 &bytes_read,
                 NULL) &&
      bytes_read == bytes_to_read;
}

// A helper function to extend the |current_buffer| of bytes such that it
// contains |desired_length| bytes read from the file given by |file_handle|.
//
// It is assumed that |file_handle| has been used to sequentially populate
// |current_buffer| thus far and is already positioned at the appropriate
// read location.
bool ReadMissingBytes(HANDLE file_handle,
                      std::vector<uint8>* current_buffer,
                      size_t desired_length) {
  DCHECK(file_handle != INVALID_HANDLE_VALUE);
  DCHECK(current_buffer != NULL);

  size_t current_length = current_buffer->size();
  if (current_length >= desired_length)
    return true;

  size_t bytes_to_read = desired_length - current_length;
  current_buffer->resize(desired_length);
  return ReadNextBytes(file_handle,
                       &(current_buffer->at(current_length)),
                       bytes_to_read);
}

// Return a |percentage| of the number of initialized bytes in the given
// |section|.
//
// This returns a percentage of the lesser of the size of the raw data in
// the section and the virtual size of the section.
//
// Note that sections can have their tails implicitly initialized to zero
// (i.e., their virtual size is larger than the raw size) and that raw data
// is padded to the PE page size if the entire section is initialized (i.e.,
// their raw data size will be larger than the virtual size).
//
// Any data after the initialized portion of the section will be soft-faulted
// in (very quickly) as needed, so we don't need to include it in the returned
// length.
size_t GetPercentageOfSectionLength(const IMAGE_SECTION_HEADER* section,
                                    size_t percentage) {
  DCHECK(section != NULL);
  DCHECK_GT(percentage, 0u);
  DCHECK_LE(percentage, kOneHundredPercent);

  size_t initialized_length = std::min(section->SizeOfRawData,
                                       section->Misc.VirtualSize);

  if (initialized_length == 0)
    return 0;

  size_t length = (initialized_length * percentage) / kOneHundredPercent;

  return std::max<size_t>(length, 1);
}

// Helper function to read through a |percentage| of the given |section|
// of the file denoted by |file_handle|. The |temp_buffer| is (re)used as
// a transient storage area as the section is read in chunks of
// |temp_buffer_size| bytes.
bool ReadThroughSection(HANDLE file_handle,
                        const IMAGE_SECTION_HEADER* section,
                        size_t percentage,
                        void* temp_buffer,
                        size_t temp_buffer_size) {
  DCHECK(file_handle != INVALID_HANDLE_VALUE);
  DCHECK(section != NULL);
  DCHECK_LE(percentage, kOneHundredPercent);
  DCHECK(temp_buffer != NULL);
  DCHECK(temp_buffer_size > 0);

  size_t bytes_to_read = GetPercentageOfSectionLength(section, percentage);
  if (bytes_to_read == 0)
    return true;

  if (!SetFilePointer(file_handle, section->PointerToRawData))
    return false;

  // Read all chunks except the last one.
  while (bytes_to_read > temp_buffer_size) {
    if (!ReadNextBytes(file_handle, temp_buffer, temp_buffer_size))
      return false;
    bytes_to_read -= temp_buffer_size;
  }

  // Read the last (possibly partial) chunk and return.
  DCHECK(bytes_to_read > 0);
  DCHECK(bytes_to_read <= temp_buffer_size);
  return ReadNextBytes(file_handle, temp_buffer, bytes_to_read);
}

// A helper function to touch all pages in the range
// [base_addr, base_addr + length).
void TouchPagesInRange(void* base_addr, size_t length) {
  DCHECK(base_addr != NULL);
  DCHECK(length > 0);

  // Get the system info so we know the page size. Also, make sure we use a
  // non-zero value for the page size; GetSystemInfo() is hookable/patchable,
  // and you never know what shenanigans someone could get up to.
  SYSTEM_INFO system_info = {};
  GetSystemInfo(&system_info);
  if (system_info.dwPageSize == 0)
    system_info.dwPageSize = 4096;

  // We don't want to read outside the byte range (which could trigger an
  // access violation), so let's figure out the exact locations of the first
  // and final bytes we want to read.
  volatile uint8* touch_ptr = reinterpret_cast<uint8*>(base_addr);
  volatile uint8* final_touch_ptr = touch_ptr + length - 1;

  // Read the memory in the range [touch_ptr, final_touch_ptr] with a stride
  // of the system page size, to ensure that it's been paged in.
  uint8 dummy;
  while (touch_ptr < final_touch_ptr) {
    dummy = *touch_ptr;
    touch_ptr += system_info.dwPageSize;
  }
  dummy = *final_touch_ptr;
}

}  // namespace

bool ImagePreReader::PartialPreReadImageOnDisk(const wchar_t* file_path,
                                               size_t percentage,
                                               size_t max_chunk_size) {
  // TODO(rogerm): change this to have the number of bytes pre-read per
  //     section be driven by a static table within the PE file (defaulting to
  //     full read if it's not there?) that's initialized by the optimization
  //     toolchain.
  DCHECK(file_path != NULL);

  if (percentage == 0)
    return true;

  if (percentage > kOneHundredPercent)
    percentage = kOneHundredPercent;

  // Validate/setup max_chunk_size, imposing a 1MB minimum on the chunk size.
  const size_t kMinChunkSize = 1024 * 1024;
  max_chunk_size = std::max(max_chunk_size, kMinChunkSize);

  // Open the file.
  base::win::ScopedHandle file(
      CreateFile(file_path,
                 GENERIC_READ,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                 NULL,
                 OPEN_EXISTING,
                 FILE_FLAG_SEQUENTIAL_SCAN,
                 NULL));

  if (!file.IsValid())
    return false;

  // Allocate a resizable buffer for the headers. We initially reserve as much
  // space as we typically see as the header size for chrome.dll and other
  // PE images.
  std::vector<uint8> headers;
  headers.reserve(kMinHeaderBufferSize);

  // Read, hopefully, all of the headers.
  if (!ReadMissingBytes(file.Get(), &headers, kMinHeaderBufferSize))
    return false;

  // The DOS header starts at offset 0 and allows us to get the offset of the
  // NT headers. Let's ensure we've read enough to capture the NT headers.
  size_t nt_headers_start =
      reinterpret_cast<IMAGE_DOS_HEADER*>(&headers[0])->e_lfanew;
  size_t nt_headers_end = nt_headers_start + sizeof(IMAGE_NT_HEADERS);
  if (!ReadMissingBytes(file.Get(), &headers, nt_headers_end))
    return false;

  // Now that we've got the NT headers we can get the total header size,
  // including all of the section headers. Let's ensure we've read enough
  // to capture all of the header data.
  size_t size_of_headers = reinterpret_cast<IMAGE_NT_HEADERS*>(
      &headers[nt_headers_start])->OptionalHeader.SizeOfHeaders;
  if (!ReadMissingBytes(file.Get(), &headers, size_of_headers))
    return false;

  // Now we have all of the headers. This is enough to let us use the PEImage
  // wrapper to query the structure of the image.
  base::win::PEImage pe_image(reinterpret_cast<HMODULE>(&headers[0]));
  CHECK(pe_image.VerifyMagic());

  // Allocate a buffer to hold the pre-read bytes.
  scoped_ptr<uint8, VirtualFreeDeleter> buffer(
      static_cast<uint8*>(
          ::VirtualAlloc(NULL, max_chunk_size, MEM_COMMIT, PAGE_READWRITE)));
  if (buffer.get() == NULL)
    return false;

  // Iterate over each section, reading in a percentage of each.
  const IMAGE_SECTION_HEADER* section = NULL;
  for (UINT i = 0; (section = pe_image.GetSectionHeader(i)) != NULL; ++i) {
    CHECK_LE(reinterpret_cast<const uint8*>(section + 1),
             &headers[0] + headers.size());
    if (!ReadThroughSection(file.Get(), section, percentage, buffer.get(),
                            max_chunk_size)) {
      return false;
    }
  }

  // We're done.
  return true;
}

bool ImagePreReader::PartialPreReadImageInMemory(const wchar_t* file_path,
                                                 size_t percentage) {
  // TODO(rogerm): change this to have the number of bytes pre-read per
  //     section be driven by a static table within the PE file (defaulting to
  //     full read if it's not there?) that's initialized by the optimization
  //     toolchain.
  DCHECK(file_path != NULL);

  if (percentage == 0)
    return true;

  if (percentage > kOneHundredPercent)
    percentage = kOneHundredPercent;

  HMODULE dll_module = ::LoadLibraryExW(
      file_path,
      NULL,
      LOAD_WITH_ALTERED_SEARCH_PATH | DONT_RESOLVE_DLL_REFERENCES);

  if (!dll_module)
    return false;

  base::win::PEImage pe_image(dll_module);
  CHECK(pe_image.VerifyMagic());

  // Iterate over each section, stepping through a percentage of each to page
  // it in off the disk.
  const IMAGE_SECTION_HEADER* section = NULL;
  for (UINT i = 0; (section = pe_image.GetSectionHeader(i)) != NULL; ++i) {
    // Get the extent we want to touch.
    size_t length = GetPercentageOfSectionLength(section, percentage);
    if (length == 0)
      continue;
    uint8* start =
        static_cast<uint8*>(pe_image.RVAToAddr(section->VirtualAddress));

    // Verify that the extent we're going to touch falls inside the section
    // we expect it to (and by implication, inside the pe_image).
    CHECK_EQ(section,
             pe_image.GetImageSectionFromAddr(start));
    CHECK_EQ(section,
             pe_image.GetImageSectionFromAddr(start + length - 1));

    // Page in the section range.
    TouchPagesInRange(start, length);
  }

  FreeLibrary(dll_module);

  return true;
}

bool ImagePreReader::PreReadImage(const wchar_t* file_path,
                                  size_t size_to_read,
                                  size_t step_size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (base::win::GetVersion() > base::win::VERSION_XP) {
    // Vista+ branch. On these OSes, the forced reads through the DLL actually
    // slows warm starts. The solution is to sequentially read file contents
    // with an optional cap on total amount to read.
    base::win::ScopedHandle file(
        CreateFile(file_path,
                   GENERIC_READ,
                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                   NULL,
                   OPEN_EXISTING,
                   FILE_FLAG_SEQUENTIAL_SCAN,
                   NULL));

    if (!file.IsValid())
      return false;

    // Default to 1MB sequential reads.
    const DWORD actual_step_size = std::max(static_cast<DWORD>(step_size),
                                            static_cast<DWORD>(1024*1024));
    LPVOID buffer = ::VirtualAlloc(NULL,
                                   actual_step_size,
                                   MEM_COMMIT,
                                   PAGE_READWRITE);

    if (buffer == NULL)
      return false;

    DWORD len;
    size_t total_read = 0;
    while (::ReadFile(file.Get(), buffer, actual_step_size, &len, NULL) &&
           len > 0 &&
           (size_to_read ? total_read < size_to_read : true)) {
      total_read += static_cast<size_t>(len);
    }
    ::VirtualFree(buffer, 0, MEM_RELEASE);
  } else {
    // WinXP branch. Here, reading the DLL from disk doesn't do
    // what we want so instead we pull the pages into memory by loading
    // the DLL and touching pages at a stride. We use the system's page
    // size as the stride, ignoring the passed in step_size, to make sure
    // each page in the range is touched.
    HMODULE dll_module = ::LoadLibraryExW(
        file_path,
        NULL,
        LOAD_WITH_ALTERED_SEARCH_PATH | DONT_RESOLVE_DLL_REFERENCES);

    if (!dll_module)
      return false;

    base::win::PEImage pe_image(dll_module);
    CHECK(pe_image.VerifyMagic());

    // We don't want to read past the end of the module (which could trigger
    // an access violation), so make sure to check the image size.
    PIMAGE_NT_HEADERS nt_headers = pe_image.GetNTHeaders();
    size_t dll_module_length = std::min(
        size_to_read ? size_to_read : ~0,
        static_cast<size_t>(nt_headers->OptionalHeader.SizeOfImage));

    // Page in then release the module.
    TouchPagesInRange(dll_module, dll_module_length);
    FreeLibrary(dll_module);
  }

  return true;
}

bool ImagePreReader::PartialPreReadImage(const wchar_t* file_path,
                                         size_t percentage,
                                         size_t max_chunk_size) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (percentage >= kOneHundredPercent) {
    // If we're reading the whole image, we don't need to parse headers and
    // navigate sections, the basic PreReadImage() can be used to just step
    // blindly through the entire file / address-space.
    return PreReadImage(file_path, 0, max_chunk_size);
  }

  if (base::win::GetVersion() > base::win::VERSION_XP) {
    // Vista+ branch. On these OSes, we warm up the Image by reading its
    // file off the disk.
    return PartialPreReadImageOnDisk(file_path, percentage, max_chunk_size);
  }

  // WinXP branch. For XP, reading the image from disk doesn't do what we want
  // so instead we pull the pages into memory by loading the DLL and touching
  // initialized pages at a stride.
  return PartialPreReadImageInMemory(file_path, percentage);
}
