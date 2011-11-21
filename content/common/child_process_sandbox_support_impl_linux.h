// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_
#define CONTENT_COMMON_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_
#pragma once

#include "content/public/common/child_process_sandbox_support_linux.h"

namespace WebKit {
struct WebFontFamily;
struct WebFontRenderStyle;
}

namespace content {

// Return a font family which provides glyphs for the Unicode code points
// specified by |utf16|
//   utf16: a native-endian UTF16 string
//   num_utf16: the number of 16-bit words in |utf16|
//   preferred_locale: preferred locale identifier for the |utf16|
//
// Returns: a font family instance.
// The instance has an empty font name if the request could not be satisfied.
void GetFontFamilyForCharacters(const uint16_t* utf16,
                                size_t num_utf16,
                                const char* preferred_locale,
                                WebKit::WebFontFamily* family);

void GetRenderStyleForStrike(const char* family, int sizeAndStyle,
                             WebKit::WebFontRenderStyle* out);

};  // namespace content

#endif  // CONTENT_COMMON_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_
