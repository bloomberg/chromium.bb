// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_RESOLVED_SEARCH_TERM_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_RESOLVED_SEARCH_TERM_H_

#include <string>

#include "base/basictypes.h"

// Encapsulates the various parts of a Resolved Search Term, which tells
// Contextual Search what to search for and how that term appears in the
// surrounding text.
struct ResolvedSearchTerm {
 public:
  ResolvedSearchTerm(bool is_invalid,
                     int response_code,
                     const std::string& search_term,
                     const std::string& display_text,
                     const std::string& alternate_term,
                     bool prevent_preload,
                     int selection_start_adjust,
                     int selection_end_adjust,
                     const std::string& context_language);
  ~ResolvedSearchTerm();

  const bool is_invalid;
  const int response_code;
  const std::string& search_term;
  const std::string& display_text;
  const std::string& alternate_term;
  const bool prevent_preload;
  const int selection_start_adjust;
  const int selection_end_adjust;
  const std::string& context_language;

  DISALLOW_COPY_AND_ASSIGN(ResolvedSearchTerm);
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_RESOLVED_SEARCH_TERM_H_
