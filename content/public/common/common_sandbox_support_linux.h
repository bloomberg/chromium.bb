// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_COMMON_SANDBOX_SUPPORT_LINUX_H_
#define CONTENT_PUBLIC_COMMON_COMMON_SANDBOX_SUPPORT_LINUX_H_

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "content/common/content_export.h"

namespace content {

// GetFontTable loads a specified font table from an open SFNT file.
//   fd: a file descriptor to the SFNT file. The position doesn't matter.
//   table_tag: the table tag in *big-endian* format, or 0 for the entire font.
//   offset: offset into the table or entire font where loading should start.
//     The offset must be between 0 and 1 GB - 1.
//   output: a buffer of size output_length that gets the data.  can be 0, in
//     which case output_length will be set to the required size in bytes.
//   output_length: size of output, if it's not 0.
//
//   returns: true on success.
CONTENT_EXPORT bool GetFontTable(int fd,
                                 uint32_t table_tag,
                                 off_t offset,
                                 uint8_t* output,
                                 size_t* output_length);

// Returns a file descriptor for a shared memory segment.  The
// executable flag indicates that the caller intends to use mprotect
// with PROT_EXEC after making a mapping, but not that it intends to
// mmap with PROT_EXEC in the first place.  (Some systems, such as
// ChromeOS, disallow PROT_EXEC in mmap on /dev/shm files but do allow
// PROT_EXEC in mprotect on mappings from such files.  This function
// can yield an object that has that constraint.)
CONTENT_EXPORT int MakeSharedMemorySegmentViaIPC(size_t length,
                                                 bool executable);

// Gets the well-known file descriptor on which we expect to find the
// sandbox IPC channel.
CONTENT_EXPORT int GetSandboxFD();

};  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_COMMON_SANDBOX_SUPPORT_LINUX_H_
