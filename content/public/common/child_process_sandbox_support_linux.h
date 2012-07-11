// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PULIC_COMMON_CHILD_PROCESS_SANDBOX_SUPPORT_LINUX_H_
#define CONTENT_PULIC_COMMON_CHILD_PROCESS_SANDBOX_SUPPORT_LINUX_H_

#include <stdint.h>
#include <string>

#include "content/common/content_export.h"

namespace content {

// Returns a file descriptor for a shared memory segment.  The
// executable flag indicates that the caller intends to use mprotect
// with PROT_EXEC after making a mapping, but not that it intends to
// mmap with PROT_EXEC in the first place.  (Some systems, such as
// ChromeOS, disallow PROT_EXEC in mmap on /dev/shm files but do allow
// PROT_EXEC in mprotect on mappings from such files.  This function
// can yield an object that has that constraint.)
CONTENT_EXPORT int MakeSharedMemorySegmentViaIPC(size_t length,
                                                 bool executable);

// Return a read-only file descriptor to the font which best matches the given
// properties or -1 on failure.
//   charset: specifies the language(s) that the font must cover. See
// render_sandbox_host_linux.cc for more information.
CONTENT_EXPORT int MatchFontWithFallback(const std::string& face, bool bold,
                                         bool italic, int charset);

// GetFontTable loads a specified font table from an open SFNT file.
//   fd: a file descriptor to the SFNT file. The position doesn't matter.
//   table: the table in *big-endian* format, or 0 for the whole font file.
//   output: a buffer of size output_length that gets the data.  can be 0, in
//     which case output_length will be set to the required size in bytes.
//   output_length: size of output, if it's not 0.
//
//   returns: true on success.
CONTENT_EXPORT bool GetFontTable(int fd, uint32_t table, uint8_t* output,
                                 size_t* output_length);

};  // namespace content

#endif  // CONTENT_PULIC_COMMON_CHILD_PROCESS_SANDBOX_SUPPORT_LINUX_H_
