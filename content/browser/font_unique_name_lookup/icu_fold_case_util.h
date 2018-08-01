// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_ICU_FOLD_CASE_UTIL_H_
#define CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_ICU_FOLD_CASE_UTIL_H_

#include "content/common/content_export.h"
#include "third_party/icu/source/common/unicode/unistr.h"

namespace content {

// Executes ICU's UnicodeString locale-independent foldCase method on
// |name_request| and returns a case folded string suitable for case-insensitive
// bitwise comparison. Used by FontTableMatcher and FontUniqueNameLookup for
// storing and comparing case folded font names.
std::string CONTENT_EXPORT IcuFoldCase(const std::string& name_request);

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_ICU_FOLD_CASE_UTIL
