// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_DETECT_DESKTOP_SEARCH_WIN_H_
#define COMPONENTS_SEARCH_ENGINES_DETECT_DESKTOP_SEARCH_WIN_H_

#include "base/strings/string16.h"

class GURL;
class SearchTermsData;

// Detects whether a URL comes from a Windows Desktop search. If so, puts the
// search terms in |search_terms| and returns true.
bool DetectWindowsDesktopSearch(const GURL& url,
                                const SearchTermsData& search_terms_data,
                                base::string16* search_terms);

#endif  // COMPONENTS_SEARCH_ENGINES_DETECT_DESKTOP_SEARCH_WIN_H_
