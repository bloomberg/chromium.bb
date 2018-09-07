// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_
#define CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_

#include <stdint.h>

#include "components/services/font/public/cpp/font_loader.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {
struct OutOfProcessFont;
struct WebFontRenderStyle;
}

namespace content {

// Returns a font family which provides glyphs for the Unicode code point
// specified by |character|, a UTF-32 character. |preferred_locale| contains the
// preferred locale identifier for |character|. The instance has an empty font
// name if the request could not be satisfied.
void GetFallbackFontForCharacter(sk_sp<font_service::FontLoader> font_loader,
                                 const int32_t character,
                                 const char* preferred_locale,
                                 blink::OutOfProcessFont* family);

// Returns rendering settings for a provided font family, size, and style.
// |size_and_style| stores the bold setting in its least-significant bit, the
// italic setting in its second-least-significant bit, and holds the requested
// size in pixels into its remaining bits.
void GetRenderStyleForStrike(sk_sp<font_service::FontLoader> font_loader,
                             const char* family,
                             int size,
                             bool is_bold,
                             bool is_italic,
                             float device_scale_factor,
                             blink::WebFontRenderStyle* out);

// Matches a font uniquely by postscript name or full font name.  Used in Blink
// for @font-face { src: local(arg) } matching.  Provide full font name or
// postscript name as argument font_unique_name in UTF-8. fallback_font contains
// a filename and fontconfig interface id if a match was found. The filename is
// empty and the interface id is zero if no match is found.
void MatchFontByPostscriptNameOrFullFontName(
    sk_sp<font_service::FontLoader> font_loader,
    const char* font_unique_name,
    blink::OutOfProcessFont* uniquely_matched_font);

};  // namespace content

#endif  // CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_
