// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/search/instant_types.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

struct TestData {
  const char* search_request_url;
  const char* expected_search_query;
  const char* expected_original_query;
  const char* expected_rlz_param;
  const char* expected_input_encoding;
  const char* expected_assisted_query_stats;
};

TEST(EmbeddedSearchRequestParams, ExtractParams) {
  TestData cases[] = {
    {"https://foo/search?q=google&oq=g&rlz=30ls&ie=utf-8&aqs=chrome..6l5.j04",
     "google",
     "g",
     "30ls",
     "utf-8",
     "chrome..6l5.j04"
    },
    // Do not populate "rlz" param.
    {"https://foo/search?q=google%20j&oq=g&ie=utf-8&aqs=chrome.2.65.j04",
     "google j",
     "g",
     "",
     "utf-8",
     "chrome.2.65.j04"
    },
    // Unescape search query.
    {"https://foo/search?q=google+j&oq=g&rlz=30&ie=utf-8&aqs=chrome.2.65.j04",
     "google j",
     "g",
     "30",
     "utf-8",
     "chrome.2.65.j04"
    },
    // Unescape original query.
    {"https://foo/search?q=g+j%20j&oq=g+j&rlz=30&ie=utf-8&aqs=chrome.2.65.j04",
     "g j j",
     "g j",
     "30",
     "utf-8",
     "chrome.2.65.j04"
    },
    {"https://foo/search?q=google#q=fun&oq=f&ie=utf-8&aqs=chrome.0.1",
     "fun",
     "f",
     "",
     "utf-8",
     "chrome.0.1"
    },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    EmbeddedSearchRequestParams params(GURL(cases[i].search_request_url));
    EXPECT_EQ(cases[i].expected_search_query,
              base::UTF16ToASCII(params.search_query)) << "For index: " << i;
    EXPECT_EQ(cases[i].expected_original_query,
              base::UTF16ToASCII(params.original_query)) << "For index: " << i;
    EXPECT_EQ(cases[i].expected_rlz_param,
              base::UTF16ToASCII(params.rlz_parameter_value)) <<
        "For index: " << i;
    EXPECT_EQ(cases[i].expected_input_encoding,
              base::UTF16ToASCII(params.input_encoding)) << "For index: " << i;
    EXPECT_EQ(cases[i].expected_assisted_query_stats,
              base::UTF16ToASCII(params.assisted_query_stats)) <<
        "For index: " << i;
  }
}
