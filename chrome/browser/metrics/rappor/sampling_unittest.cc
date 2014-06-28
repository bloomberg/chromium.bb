// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/rappor/sampling.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace rappor {

// Test that extracting the sample works correctly for different schemes.
TEST(RapporSamplingTest, GetDomainAndRegistrySampleFromGURLTest) {
  EXPECT_EQ("google.com", GetDomainAndRegistrySampleFromGURL(
      GURL("https://www.GoOgLe.com:80/blah")));
  EXPECT_EQ("file://", GetDomainAndRegistrySampleFromGURL(
      GURL("file://foo/bar/baz")));
  EXPECT_EQ("chrome-extension://abc1234", GetDomainAndRegistrySampleFromGURL(
      GURL("chrome-extension://abc1234/foo.html")));
  EXPECT_EQ("chrome-search://local-ntp", GetDomainAndRegistrySampleFromGURL(
      GURL("chrome-search://local-ntp/local-ntp.html")));
}

// Make sure recording a sample during tests, when the Rappor service is NULL,
// doesn't cause a crash.
TEST(RapporSamplingTest, SmokeTest) {
  SampleDomainAndRegistryFromGURL(std::string(), GURL());
}

}  // namespace rappor
