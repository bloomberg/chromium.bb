// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_http_header_provider.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chrome_variations {

TEST(VariationsHttpHeaderProviderTest, ShouldAppendHeaders) {
  struct {
    const char* url;
    bool should_append_headers;
  } cases[] = {
    { "http://google.com", true },
    { "http://www.google.com", true },
    { "http://m.google.com", true },
    { "http://google.ca", true },
    { "https://google.ca", true },
    { "http://google.co.uk", true },
    { "http://google.co.uk:8080/", true },
    { "http://www.google.co.uk:8080/", true },
    { "http://youtube.com", true },
    { "http://www.youtube.com", true },
    { "http://www.youtube.ca", true },
    { "http://www.youtube.co.uk:8080/", true },
    { "https://www.youtube.com", true },
    { "http://www.yahoo.com", false },
    { "http://youtube", false },
    { "http://google", false },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    const GURL url(cases[i].url);
    EXPECT_EQ(cases[i].should_append_headers,
              VariationsHttpHeaderProvider::ShouldAppendHeaders(url)) << url;
  }
}

}  // namespace chrome_variations
