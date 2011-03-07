// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_SANDBOX_SUPPORT_LINUX_H_
#define CHROME_RENDERER_RENDERER_SANDBOX_SUPPORT_LINUX_H_
#pragma once

#include <stdint.h>

#include <string>

namespace WebKit {
struct WebFontRenderStyle;
}

namespace renderer_sandbox_support {

// Return a font family which provides glyphs for the Unicode code points
// specified by |utf16|
//   utf16: a native-endian UTF16 string
//   num_utf16: the number of 16-bit words in |utf16|
//   preferred_locale: preferred locale identifier for the |utf16|
//
// Returns: the font family or an empty string if the request could not be
// satisfied.
std::string getFontFamilyForCharacters(const uint16_t* utf16,
                                       size_t num_utf16,
                                       const char* preferred_locale);

void getRenderStyleForStrike(const char* family, int sizeAndStyle,
                             WebKit::WebFontRenderStyle* out);

// Returns a file descriptor for a shared memory segment.
// The second argument is ignored because SHM segments are always
// mappable with PROT_EXEC on Linux.
int MakeSharedMemorySegmentViaIPC(size_t length, bool executable);

// Return a read-only file descriptor to the font which best matches the given
// properties or -1 on failure.
//   charset: specifies the language(s) that the font must cover. See
// render_sandbox_host_linux.cc for more information.
int MatchFontWithFallback(const std::string& face, bool bold,
                          bool italic, int charset);

// GetFontTable loads a specified font table from an open SFNT file.
//   fd: a file descriptor to the SFNT file. The position doesn't matter.
//   table: the table in *big-endian* format, or 0 for the whole font file.
//   output: a buffer of size output_length that gets the data.  can be 0, in
//     which case output_length will be set to the required size in bytes.
//   output_length: size of output, if it's not 0.
//
//   returns: true on success.
bool GetFontTable(int fd, uint32_t table, uint8_t* output,
                  size_t* output_length);

};  // namespace render_sandbox_support

#endif  // CHROME_RENDERER_RENDERER_SANDBOX_SUPPORT_LINUX_H_
