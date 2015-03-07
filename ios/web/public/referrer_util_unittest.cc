// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/referrer_util.h"

#include "ios/web/public/referrer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char* test_urls[] = {
    "",
    "http://insecure.com/foo/bar.html",
    "https://secure.net/trustworthy.html",
};

enum { Empty = 0, Insecure, Secure };

TEST(ReferrerUtilTest, Sanitization) {
  GURL unsanitized("http://user:password@foo.com/bar/baz.html#fragment");
  GURL sanitized = web::ReferrerForHeader(unsanitized);
  EXPECT_EQ("http://foo.com/bar/baz.html", sanitized.spec());
}

TEST(ReferrerUtilTest, DefaultPolicy) {
  // Default: all but secure->insecure should have a full referrer.
  for (unsigned int source = Empty; source < arraysize(test_urls); ++source) {
    for (unsigned int dest = Insecure; dest < arraysize(test_urls); ++dest) {
      web::Referrer referrer(GURL(test_urls[source]),
                             web::ReferrerPolicyDefault);
      std::string value = web::ReferrerHeaderValueForNavigation(
          GURL(test_urls[dest]), referrer);
      if (source == Empty)
        EXPECT_EQ("", value);
      else if (source == Secure && dest == Insecure)
        EXPECT_EQ("", value);
      else
        EXPECT_EQ(test_urls[source], value);

      net::URLRequest::ReferrerPolicy policy =
          web::PolicyForNavigation(GURL(test_urls[dest]), referrer);
      EXPECT_EQ(
          net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
          policy);
    }
  }
}

TEST(ReferrerUtilTest, NeverPolicy) {
  // Never: always empty string.
  for (unsigned int source = Empty; source < arraysize(test_urls); ++source) {
    for (unsigned int dest = Insecure; dest < arraysize(test_urls); ++dest) {
      web::Referrer referrer(GURL(test_urls[source]),
                             web::ReferrerPolicyNever);
      std::string value = web::ReferrerHeaderValueForNavigation(
          GURL(test_urls[dest]), referrer);
      EXPECT_EQ("", value);

      net::URLRequest::ReferrerPolicy policy =
          web::PolicyForNavigation(GURL(test_urls[dest]), referrer);
      EXPECT_EQ(
          net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
          policy);
    }
  }
}

TEST(ReferrerUtilTest, AlwaysPolicy) {
  // Always: always the full referrer.
  for (unsigned int source = Empty; source < arraysize(test_urls); ++source) {
    for (unsigned int dest = Insecure; dest < arraysize(test_urls); ++dest) {
      web::Referrer referrer(GURL(test_urls[source]),
                             web::ReferrerPolicyAlways);
      std::string value = web::ReferrerHeaderValueForNavigation(
          GURL(test_urls[dest]), referrer);
      EXPECT_EQ(test_urls[source], value);

      net::URLRequest::ReferrerPolicy policy =
          web::PolicyForNavigation(GURL(test_urls[dest]), referrer);
      if (source == Empty) {
        EXPECT_EQ(net::URLRequest::
                      CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
                  policy);
      } else {
        EXPECT_EQ(net::URLRequest::NEVER_CLEAR_REFERRER, policy);
      }
    }
  }
}

TEST(ReferrerUtilTest, OriginPolicy) {
  // Always: always just the origin.
  for (unsigned int source = Empty; source < arraysize(test_urls); ++source) {
    for (unsigned int dest = Insecure; dest < arraysize(test_urls); ++dest) {
      web::Referrer referrer(GURL(test_urls[source]),
                             web::ReferrerPolicyOrigin);
      std::string value = web::ReferrerHeaderValueForNavigation(
          GURL(test_urls[dest]), referrer);
      EXPECT_EQ(GURL(test_urls[source]).GetOrigin().spec(), value);

      net::URLRequest::ReferrerPolicy policy =
          web::PolicyForNavigation(GURL(test_urls[dest]), referrer);
      if (source == Empty) {
        EXPECT_EQ(net::URLRequest::
                      CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
                  policy);
      } else {
        EXPECT_EQ(net::URLRequest::NEVER_CLEAR_REFERRER, policy);
      }
    }
  }
}

}  // namespace
