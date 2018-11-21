// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/identity_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class IdentityUtilsTest : public testing::Test {};

const char kUsername[] = "test@test.com";

const char kValidWildcardPattern[] = ".*@test.com";
const char kInvalidWildcardPattern[] = "*@test.com";

const char kMatchingPattern1[] = "test@test.com";
const char kMatchingPattern2[] = ".*@test.com";
const char kMatchingPattern3[] = "test@.*.com";
const char kMatchingPattern4[] = ".*@.*.com";
const char kMatchingPattern5[] = ".*@.*";
const char kMatchingPattern6[] = ".*";

const char kNonMatchingPattern[] = ".*foo.*";
const char kNonMatchingUsernamePattern[] = "foo@test.com";
const char kNonMatchingDomainPattern[] = "test@foo.com";

TEST_F(IdentityUtilsTest, IsUsernameAllowedByPattern_EmptyPatterns) {
  EXPECT_TRUE(identity::IsUsernameAllowedByPattern(kUsername, ""));
  EXPECT_FALSE(identity::IsUsernameAllowedByPattern(kUsername, "   "));
}

TEST_F(IdentityUtilsTest, IsUsernameAllowedByPattern_InvalidWildcardPatterns) {
  // identity::IsUsernameAllowedByPattern should recognize invalid wildcard
  // patterns like "*@foo.com" and insert a "." before them automatically.
  EXPECT_TRUE(
      identity::IsUsernameAllowedByPattern(kUsername, kValidWildcardPattern));
  EXPECT_TRUE(
      identity::IsUsernameAllowedByPattern(kUsername, kInvalidWildcardPattern));
}

TEST_F(IdentityUtilsTest, IsUsernameAllowedByPattern_MatchingWildcardPatterns) {
  EXPECT_TRUE(
      identity::IsUsernameAllowedByPattern(kUsername, kMatchingPattern1));
  EXPECT_TRUE(
      identity::IsUsernameAllowedByPattern(kUsername, kMatchingPattern2));
  EXPECT_TRUE(
      identity::IsUsernameAllowedByPattern(kUsername, kMatchingPattern3));
  EXPECT_TRUE(
      identity::IsUsernameAllowedByPattern(kUsername, kMatchingPattern4));
  EXPECT_TRUE(
      identity::IsUsernameAllowedByPattern(kUsername, kMatchingPattern5));
  EXPECT_TRUE(
      identity::IsUsernameAllowedByPattern(kUsername, kMatchingPattern6));

  EXPECT_FALSE(
      identity::IsUsernameAllowedByPattern(kUsername, kNonMatchingPattern));
  EXPECT_FALSE(identity::IsUsernameAllowedByPattern(
      kUsername, kNonMatchingUsernamePattern));
  EXPECT_FALSE(identity::IsUsernameAllowedByPattern(kUsername,
                                                    kNonMatchingDomainPattern));
}
