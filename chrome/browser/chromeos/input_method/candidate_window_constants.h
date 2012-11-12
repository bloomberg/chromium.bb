// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONSTANTS_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONSTANTS_H_

namespace chromeos {
namespace input_method {

// Colors used in the candidate window UI.
const SkColor kFrameColor = SkColorSetRGB(0x96, 0x96, 0x96);
const SkColor kShortcutBackgroundColor = SkColorSetARGB(0x10, 0x3, 0x4, 0xf);
const SkColor kSelectedRowBackgroundColor = SkColorSetRGB(0xd1, 0xea, 0xff);
const SkColor kDefaultBackgroundColor = SkColorSetRGB(0xff, 0xff, 0xff);
const SkColor kSelectedRowFrameColor = SkColorSetRGB(0x7f, 0xac, 0xdd);
const SkColor kFooterTopColor = SkColorSetRGB(0xff, 0xff, 0xff);
const SkColor kFooterBottomColor = SkColorSetRGB(0xee, 0xee, 0xee);
const SkColor kShortcutColor = SkColorSetRGB(0x61, 0x61, 0x61);
const SkColor kDisabledShortcutColor = SkColorSetRGB(0xcc, 0xcc, 0xcc);
const SkColor kAnnotationColor = SkColorSetRGB(0x88, 0x88, 0x88);
const SkColor kSelectedInfolistRowBackgroundColor =
    SkColorSetRGB(0xee, 0xee, 0xee);
const SkColor kSelectedInfolistRowFrameColor = SkColorSetRGB(0xcc, 0xcc, 0xcc);
const SkColor kInfolistTitleBackgroundColor = SkColorSetRGB(0xdd, 0xdd, 0xdd);

// We'll use a bigger font size, so Chinese characters are more readable
// in the candidate window.
const int kFontSizeDelta = 2;

// Currently the infolist window only supports Japanese font.
#if defined(GOOGLE_CHROME_BUILD)
const char kJapaneseFontName[] = "MotoyaG04Gothic";
#else
const char kJapaneseFontName[] = "IPAPGothic";
#endif

// The minimum width of candidate labels in the vertical candidate
// window. We use this value to prevent the candidate window from being
// too narrow when all candidates are short.
const int kMinCandidateLabelWidth = 100;
// The maximum width of candidate labels in the vertical candidate
// window. We use this value to prevent the candidate window from being
// too wide when one of candidates are long.
const int kMaxCandidateLabelWidth = 500;
// The minimum width of preedit area. We use this value to prevent the
// candidate window from being too narrow when candidate lists are not shown.
const int kMinPreeditAreaWidth = 134;

// The width of the infolist indicator icon in the candidate window.
const int kInfolistIndicatorIconWidth = 4;
// The padding size of the infolist indicator icon in the candidate window.
const int kInfolistIndicatorIconPadding = 2;

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONSTANTS_H_
