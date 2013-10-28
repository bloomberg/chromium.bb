// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_PAGE_TRANSLATED_DETAILS_H_
#define CHROME_BROWSER_TRANSLATE_PAGE_TRANSLATED_DETAILS_H_

#include <string>

#include "chrome/common/translate/translate_errors.h"

// Used when sending a notification about a page that has been translated.
struct PageTranslatedDetails {
  std::string source_language;
  std::string target_language;
  TranslateErrors::Type error_type;
};

#endif  // CHROME_BROWSER_TRANSLATE_PAGE_TRANSLATED_DETAILS_H_
