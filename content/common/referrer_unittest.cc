// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "content/public/common/referrer.h"

namespace {

const char* kTestURL = "http://google.com/";

// Check that copying Referrer objects works.
TEST(ReferrerTest, Copy) {
  content::Referrer r1(GURL(kTestURL), WebKit::WebReferrerPolicyAlways);
  content::Referrer r2;

  r2 = r1;
  EXPECT_EQ(GURL(kTestURL), r2.url);
  EXPECT_EQ(WebKit::WebReferrerPolicyAlways, r2.policy);
}

}  // namespace
