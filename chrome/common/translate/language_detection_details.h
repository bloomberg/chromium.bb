// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_TRANSLATE_LANGUAGE_DETECTION_DETAILS_H_
#define CHROME_COMMON_TRANSLATE_LANGUAGE_DETECTION_DETAILS_H_

#include <string>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "url/gurl.h"

struct LanguageDetectionDetails {
  LanguageDetectionDetails();
  ~LanguageDetectionDetails();

  // The time when this was created.
  base::Time time;

  // The URL.
  GURL url;

  // The language detected by the content (Content-Language).
  std::string content_language;

  // The language detected by CLD.
  std::string cld_language;

  // Whether the CLD detection is reliable or not.
  bool is_cld_reliable;

  // The language written in the lang attribute of the html element.
  std::string html_root_language;

  // The adopted language.
  std::string adopted_language;

  // The contents which is used for detection.
  string16 contents;
};

#endif  // CHROME_COMMON_TRANSLATE_LANGUAGE_DETECTION_DETAILS_H_
