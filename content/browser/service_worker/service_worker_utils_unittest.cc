// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(ServiceWorkerUtilsTest, ScopeMatches) {
  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*"), GURL("http://www.example.com/")));
  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/page.html")));

  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*"), GURL("https://www.example.com/")));
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*"),
      GURL("https://www.example.com/page.html")));

  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*"), GURL("http://www.foo.com/")));
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*"), GURL("https://www.foo.com/page.html")));
}

}  // namespace content
