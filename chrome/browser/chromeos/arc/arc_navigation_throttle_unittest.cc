// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_navigation_throttle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace arc {

TEST(ArcNavigationThrottleTest, TestShouldOverrideUrlLoading) {
  // A navigation within the same domain shouldn't be overridden.
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://google.com"), GURL("http://google.com/")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://google.com"), GURL("http://a.google.com/")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.google.com"), GURL("http://google.com/")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.google.com"), GURL("http://b.google.com/")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.google.com"), GURL("http://b.c.google.com/")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.b.google.com"), GURL("http://c.google.com/")));

  // If either of two paramters is empty, the function should return false.
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL(), GURL("http://a.google.com/")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.google.com/"), GURL()));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL(), GURL()));

  // A navigation not within the same domain can be overridden.
  EXPECT_TRUE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.google.com"), GURL("http://www.not-google.com/")));
  EXPECT_TRUE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.not-google.com"), GURL("http://www.google.com/")));
}

}  // namespace arc
