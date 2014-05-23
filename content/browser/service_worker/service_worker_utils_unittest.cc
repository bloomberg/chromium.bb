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

  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/"), GURL("http://www.example.com/")));
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/"), GURL("http://www.example.com/x")));

  // '?' is not a wildcard.
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/?"), GURL("http://www.example.com/x")));
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/?"), GURL("http://www.example.com/")));
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/?"), GURL("http://www.example.com/xx")));
  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/?"), GURL("http://www.example.com/?")));

  // Query string is part of the resource.
  ASSERT_TRUE(
      ServiceWorkerUtils::ScopeMatches(GURL("http://www.example.com/?a=b"),
                                       GURL("http://www.example.com/?a=b")));
  ASSERT_TRUE(
      ServiceWorkerUtils::ScopeMatches(GURL("http://www.example.com/?a=*"),
                                       GURL("http://www.example.com/?a=b")));
  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*"), GURL("http://www.example.com/?a=b")));
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/"), GURL("http://www.example.com/?a=b")));

  // '*' only has special meaning in terminal position.
  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*/x"), GURL("http://www.example.com/*/x")));
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*/x"), GURL("http://www.example.com/a/x")));
  ASSERT_FALSE(
      ServiceWorkerUtils::ScopeMatches(GURL("http://www.example.com/*/x/*"),
                                       GURL("http://www.example.com/a/x/b")));
  ASSERT_TRUE(
      ServiceWorkerUtils::ScopeMatches(GURL("http://www.example.com/*/x/*"),
                                       GURL("http://www.example.com/*/x/b")));

  // URLs canonicalize \ to / so this is equivalent to  "...//*" and "...//x"
  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/\\*"), GURL("http://www.example.com/\\x")));
}

}  // namespace content
