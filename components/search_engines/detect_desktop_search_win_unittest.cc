// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/detect_desktop_search_win.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/search_engines/testing_search_terms_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
struct DetectWindowsDesktopSearchTestData {
  const char* url;
  const wchar_t* expected_search_terms;
};
}  // namespace

TEST(DetectWindowsDesktopSearch, DetectWindowsDesktopSearch) {
  const DetectWindowsDesktopSearchTestData test_data[] = {
      {"https://www.google.com", L""},
      {"https://www.bing.com/search", L""},
      {"https://www.bing.com/search?q=keyword&form=QBLH", L""},
      {"https://www.bing.com/search?q=keyword&form=WNSGPH", L"keyword"},
      {"https://www.bing.com/search?q=keyword&form=WNSBOX", L"keyword"},
      {"https://www.bing.com/search?q=keyword&FORM=WNSGPH", L"keyword"},
      {"https://www.bing.com/search?q=keyword&FORM=WNSBOX", L"keyword"},
      {"https://www.bing.com/search?form=WNSGPH&q=keyword", L"keyword"},
      {"https://www.bing.com/search?q=keyword&form=WNSGPH&other=stuff",
       L"keyword"},
      {"https://www.bing.com/search?q=%C3%A8+%C3%A9&form=WNSGPH", L"\xE8 \xE9"},
  };

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    TestingSearchTermsData search_terms_data("https://www.google.com");
    base::string16 search_terms;
    bool is_desktop_search = DetectWindowsDesktopSearch(
        GURL(test_data[i].url), search_terms_data, &search_terms);
    const base::string16 expected_search_terms(
        test_data[i].expected_search_terms);
    EXPECT_EQ(!expected_search_terms.empty(), is_desktop_search);
    EXPECT_EQ(expected_search_terms, search_terms);
  }
}
