// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(IdentityLaunchWebAuthFlowTest, RedirectMatching) {
  scoped_refptr<extensions::IdentityLaunchWebAuthFlowFunction> function(
      new extensions::IdentityLaunchWebAuthFlowFunction());

  function->InitFinalRedirectURLPrefixesForTest("abcdefghij");
  // Positive cases.
  EXPECT_TRUE(function->IsFinalRedirectURL(
      GURL("https://abcdefghij.chromiumapp.org/")));
  EXPECT_TRUE(function->IsFinalRedirectURL(
      GURL("https://abcdefghij.chromiumapp.org/callback")));

  // Negative cases.
  EXPECT_FALSE(function->IsFinalRedirectURL(
      GURL("https://www.foo.com/")));
  // http scheme is not allowed.
  EXPECT_FALSE(function->IsFinalRedirectURL(
      GURL("http://abcdefghij.chromiumapp.org/callback")));
  EXPECT_FALSE(function->IsFinalRedirectURL(
      GURL("https://abcd.chromiumapp.org/callback")));
  // chrome-extension scheme is not allowed
  EXPECT_FALSE(function->IsFinalRedirectURL(
      GURL("chrome-extension://abcdefghij/")));
  EXPECT_FALSE(function->IsFinalRedirectURL(
      GURL("chrome-extension://abcdefghij/callback")));
  EXPECT_FALSE(function->IsFinalRedirectURL(
      GURL("chrome-extension://abcd/callback")));
  EXPECT_FALSE(function->IsFinalRedirectURL(
      GURL("chrome-extension://abcdefghijkl/")));
}
