// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TITLE_PREFIX_MATCHER_H_
#define CHROME_BROWSER_UI_TITLE_PREFIX_MATCHER_H_
#pragma once

#include <vector>

#include "base/string16.h"

// This class exposes a static method that receives a vector of TitleInfo
// objects so that it can find the length of the common prefixes among all
// the titles. It can be used for tab titles for example so that the common
// prefixes can be elided.
// First, the caller needs to fill a vector of TitleInfo objects with the titles
// for which they want to find the common prefix lengths. They can also provide
// an optional caller_value where the index of the tabs could be saved
// for example. This way the caller can remember which tab this title belongs
// to, if not all tabs are passed into the vector.
// When CalculatePrefixLengths returns, the TitleInfo objects in the vector
// are set with the prefix_length that is common between this title
// and at least one other.
// Note that the prefix_length is only calculated at word boundaries.
class TitlePrefixMatcher {
 public:
  struct TitleInfo {
    TitleInfo(const string16* title, int caller_value);
    // We assume the title string will be valid throughout the execution of
    // the prefix lengths calculation, and so we use a pointer to avoid an
    // unnecessary string copy.
    const string16* title;
    // This contains the number of characters at the beginning of title that
    // are common with other titles in the TitleInfo vector.
    size_t prefix_length;
    // Utility data space for the caller. Unused by CalculatePrefixLengths.
    int caller_value;
  };
  static void CalculatePrefixLengths(std::vector<TitleInfo>* title_infos);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TitlePrefixMatcher);
};

#endif  // CHROME_BROWSER_UI_TITLE_PREFIX_MATCHER_H_
