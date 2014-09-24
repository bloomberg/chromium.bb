// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/psl_matching_helper.h"

#include "base/basictypes.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

TEST(PSLMatchingUtilsTest, IsPublicSuffixDomainMatch) {
  struct TestPair {
    const char* url1;
    const char* url2;
    bool should_match;
  };

  TestPair pairs[] = {
      {"http://facebook.com", "http://facebook.com", true},
      {"http://facebook.com/path", "http://facebook.com/path", true},
      {"http://facebook.com/path1", "http://facebook.com/path2", true},
      {"http://facebook.com", "http://m.facebook.com", true},
      {"http://www.facebook.com", "http://m.facebook.com", true},
      {"http://facebook.com/path", "http://m.facebook.com/path", true},
      {"http://facebook.com/path1", "http://m.facebook.com/path2", true},
      {"http://example.com/has space", "http://example.com/has space", true},
      {"http://www.example.com", "http://wwwexample.com", false},
      {"http://www.example.com", "https://www.example.com", false},
      {"http://www.example.com:123", "http://www.example.com", false},
      {"http://www.example.org", "http://www.example.com", false},
      // Invalid urls should not match anything.
      {"http://", "http://", false},
      {"", "", false},
      {"bad url", "bad url", false},
      {"http://www.example.com", "http://", false},
      {"", "http://www.example.com", false},
      {"http://www.example.com", "bad url", false},
      {"http://www.example.com/%00", "http://www.example.com/%00", false},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(pairs); ++i) {
    autofill::PasswordForm form1;
    form1.signon_realm = pairs[i].url1;
    autofill::PasswordForm form2;
    form2.signon_realm = pairs[i].url2;
    EXPECT_EQ(pairs[i].should_match,
              IsPublicSuffixDomainMatch(form1.signon_realm, form2.signon_realm))
        << "First URL = " << pairs[i].url1
        << ", second URL = " << pairs[i].url2;
  }
}

}  // namespace

}  // namespace password_manager
