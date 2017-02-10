// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_LINUX_H_
#define CONTENT_PUBLIC_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_LINUX_H_

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "content/common/content_export.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"

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
// fallback_family: If not set to PP_BROWSERFONT_TRUSTED_FAMILY_DEFAULT, font
// selection should fall back to generic Windows font names like Arial and
// Times New Roman.
CONTENT_EXPORT int MatchFontWithFallback(
    const std::string& face,
    bool bold,
    bool italic,
    int charset,
    PP_BrowserFont_Trusted_Family fallback_family);

};  // namespace content

#endif  // CONTENT_PUBLIC_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_LINUX_H_
