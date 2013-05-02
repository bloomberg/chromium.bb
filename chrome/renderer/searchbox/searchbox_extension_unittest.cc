// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace internal {

// Defined in searchbox_extension.cc
bool IsSensitiveInput(const string16& query);

TEST(SearchboxExtensionTest, RestrictedInput) {
  // A random query prefix should be fine.
  EXPECT_FALSE(IsSensitiveInput(UTF8ToUTF16("a")));

  // An http URL.
  EXPECT_FALSE(IsSensitiveInput(UTF8ToUTF16("http://www.example.com/foo/bar")));

  // Something with an odd scheme.
  EXPECT_TRUE(IsSensitiveInput(UTF8ToUTF16("file://foo")));
  EXPECT_TRUE(IsSensitiveInput(UTF8ToUTF16("asdf://bar")));
  // Verify all caps isn't a workaround.
  EXPECT_TRUE(IsSensitiveInput(UTF8ToUTF16("FILE://foo")));

  // A url with a port is bad.
  EXPECT_TRUE(IsSensitiveInput(UTF8ToUTF16("http://www.example.com:1000")));
  EXPECT_TRUE(IsSensitiveInput(UTF8ToUTF16("http://foo:1000")));

  // A url with CGI params is bad.
  EXPECT_TRUE(IsSensitiveInput(UTF8ToUTF16("http://www.example.com/?foo=bar")));

  // A url with a hash fragment is bad.
  EXPECT_TRUE(IsSensitiveInput(
      UTF8ToUTF16("http://www.example.com/foo#foo=bar")));

  // A URL with potential login-password information is bad.
  EXPECT_TRUE(IsSensitiveInput(UTF8ToUTF16("http://foo:bar@www.example.com")));

  // An https domain is ok.
  EXPECT_FALSE(IsSensitiveInput(UTF8ToUTF16("https://foo")));
  EXPECT_FALSE(IsSensitiveInput(UTF8ToUTF16("https://www.example.com/")));

  // An https url with path components is considered sensitive.
  EXPECT_TRUE(IsSensitiveInput(UTF8ToUTF16("https://www.foo.com/bar/baz")));
}

}  // namespace internal
