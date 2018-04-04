// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/intent_helper/apps_navigation_throttle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

TEST(AppsNavigationThrottleTest, TestShouldOverrideUrlLoading) {
  // A navigation within the same domain shouldn't be overridden except if the
  // domain is google.com.
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://google.com"), GURL("http://google.com/")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://google.com"), GURL("http://a.google.com/")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.google.com"), GURL("http://google.com/")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.google.com"), GURL("http://b.google.com/")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.google.com"), GURL("http://b.c.google.com/")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.b.google.com"), GURL("http://c.google.com/")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.b.google.com"), GURL("http://b.google.com")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://b.google.com"), GURL("http://a.b.google.com")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://notg.com"), GURL("http://notg.com")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.notg.com"), GURL("http://notg.com")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://notg.com"), GURL("http://a.notg.com")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.notg.com"), GURL("http://b.notg.com")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.notg.com"), GURL("http://a.b.notg.com")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.b.notg.com"), GURL("http://c.notg.com")));

  // Same as last tests, except for "play.google.com".
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://google.com"), GURL("http://play.google.com/fake_app")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("https://www.google.com.mx"),
      GURL("https://play.google.com/fake_app")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("https://mail.google.com"),
      GURL("https://play.google.com/fake_app")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("https://play.google.com/search"),
      GURL("https://play.google.com/fake_app")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://not_google.com"), GURL("http://play.google.com/fake_app")));

  // If either of two paramters is empty, the function should return false.
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL(), GURL("http://a.google.com/")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.google.com/"), GURL()));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL(), GURL()));

  // A navigation not within the same domain can be overridden.
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.google.com"), GURL("http://www.not-google.com/")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.not-google.com"), GURL("http://www.google.com/")));

  // A navigation with neither an http nor https scheme cannot be overriden.
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("chrome-extension://fake_document"), GURL("http://www.a.com")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.a.com"), GURL("chrome-extension://fake_document")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("chrome-extension://fake_document"), GURL("https://www.a.com")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("https://www.a.com"), GURL("chrome-extension://fake_document")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("chrome-extension://fake_a"), GURL("chrome-extension://fake_b")));
}

}  // namespace chromeos
