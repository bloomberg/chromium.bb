// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

// Test for GetSavePasswordDialogTitleTextAndLinkRange().
TEST(ManagePasswordsViewUtilTest, GetSavePasswordDialogTitleTextAndLinkRange) {
  const struct {
    const char* const user_visible_url;
    const char* const form_origin_url;
    bool is_smartlock_branding_enabled;
    const char* const expected_title_text_ends_with;
    size_t expected_link_range_start;
    size_t expected_link_range_end;
  } test_cases[] = {
      // Same domains.
      {"http://example.com/landing", "http://example.com/login#form?value=3",
       false, "this site?", 0, 0},
      {"http://example.com/landing", "http://example.com/login#form?value=3",
       true, "this site?", 12, 29},

      // Different subdomains.
      {"https://a.example.com/landing",
       "https://b.example.com/login#form?value=3", false, "this site?", 0, 0},
      {"https://a.example.com/landing",
       "https://b.example.com/login#form?value=3", true, "this site?", 12, 29},

      // Different domains.
      {"https://another.org", "https://example.com:/login#form?value=3", false,
       "https://example.com?", 0, 0},
      {"https://another.org", "https://example.com/login#form?value=3", true,
       "https://example.com?", 12, 29},

      // Different domains and password form origin url with
      // default port for the scheme.
      {"https://another.org", "https://example.com:443/login#form?value=3",
       false, "https://example.com?", 0, 0},
      {"https://another.org", "http://example.com:80/login#form?value=3", true,
       "http://example.com?", 12, 29},

      // Different domains and password form origin url with
      // non-default port for the scheme.
      {"https://another.org", "https://example.com:8001/login#form?value=3",
       false, "https://example.com:8001?", 0, 0},
      {"https://another.org", "https://example.com:8001/login#form?value=3",
       true, "https://example.com:8001?", 12, 29}};

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    SCOPED_TRACE(testing::Message()
                 << "user_visible_url = " << test_cases[i].user_visible_url
                 << ", form_origin_url = " << test_cases[i].form_origin_url);

    base::string16 title;
    gfx::Range title_link_range;
    GetSavePasswordDialogTitleTextAndLinkRange(
        GURL(test_cases[i].user_visible_url),
        GURL(test_cases[i].form_origin_url),
        test_cases[i].is_smartlock_branding_enabled, &title, &title_link_range);

    // Verify against expectations.
    EXPECT_TRUE(base::EndsWith(
        title, base::ASCIIToUTF16(test_cases[i].expected_title_text_ends_with),
        false));
    EXPECT_EQ(test_cases[i].expected_link_range_start,
              title_link_range.start());
    EXPECT_EQ(test_cases[i].expected_link_range_end, title_link_range.end());
  }
}
