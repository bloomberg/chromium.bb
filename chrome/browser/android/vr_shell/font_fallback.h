// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_FONT_FALLBACK_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_FONT_FALLBACK_H_

#include <string>

#include "third_party/icu/source/common/unicode/uchar.h"

namespace gfx {
class Font;
}  // namespace gfx

namespace vr_shell {

// Return a font name which provides a glyph for the Unicode code point
// specified by character.
//   default_font: The main font for which fallbacks are required
//   c: a UTF-32 code point
//   preferred_locale: preferred locale identifier (if any) for |c|
//                     (e.g. "en", "ja", "zh-CN")
//
// Returns: the font name. The value is empty if the request could not be
// satisfied or if the provided default font supports it.
std::string GetFallbackFontNameForChar(const gfx::Font& default_font,
                                       UChar32 c,
                                       const std::string& preferred_locale);

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_FONT_FALLBACK_H_
