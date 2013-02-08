// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_auth_util.h"

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gaia {

TEST(GaiaAuthUtilTest, EmailAddressNoOp) {
  const char lower_case[] = "user@what.com";
  EXPECT_EQ(lower_case, CanonicalizeEmail(lower_case));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreCaps) {
  EXPECT_EQ(CanonicalizeEmail("user@what.com"),
            CanonicalizeEmail("UsEr@what.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreDomainCaps) {
  EXPECT_EQ(CanonicalizeEmail("user@what.com"),
            CanonicalizeEmail("UsEr@what.COM"));
}

TEST(GaiaAuthUtilTest, EmailAddressRejectOneUsernameDot) {
  EXPECT_NE(CanonicalizeEmail("u.ser@what.com"),
            CanonicalizeEmail("UsEr@what.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressMatchWithOneUsernameDot) {
  EXPECT_EQ(CanonicalizeEmail("u.ser@what.com"),
            CanonicalizeEmail("U.sEr@what.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreOneUsernameDot) {
  EXPECT_EQ(CanonicalizeEmail("us.er@gmail.com"),
            CanonicalizeEmail("UsEr@gmail.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreManyUsernameDots) {
  EXPECT_EQ(CanonicalizeEmail("u.ser@gmail.com"),
            CanonicalizeEmail("Us.E.r@gmail.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreConsecutiveUsernameDots) {
  EXPECT_EQ(CanonicalizeEmail("use.r@gmail.com"),
            CanonicalizeEmail("Us....E.r@gmail.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressDifferentOnesRejected) {
  EXPECT_NE(CanonicalizeEmail("who@what.com"),
            CanonicalizeEmail("Us....E.r@what.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnorePlusSuffix) {
  const char with_plus[] = "user+cc@what.com";
  EXPECT_EQ(with_plus, CanonicalizeEmail(with_plus));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreMultiPlusSuffix) {
  const char multi_plus[] = "user+cc+bcc@what.com";
  EXPECT_EQ(multi_plus, CanonicalizeEmail(multi_plus));
}

TEST(GaiaAuthUtilTest, CanonicalizeDomain) {
  const char domain[] = "example.com";
  EXPECT_EQ(domain, CanonicalizeDomain("example.com"));
  EXPECT_EQ(domain, CanonicalizeDomain("EXAMPLE.cOm"));
}

TEST(GaiaAuthUtilTest, ExtractDomainName) {
  const char domain[] = "example.com";
  EXPECT_EQ(domain, ExtractDomainName("who@example.com"));
  EXPECT_EQ(domain, ExtractDomainName("who@EXAMPLE.cOm"));
}

TEST(GaiaAuthUtilTest, SanitizeMissingDomain) {
  EXPECT_EQ("nodomain@gmail.com", SanitizeEmail("nodomain"));
}

TEST(GaiaAuthUtilTest, SanitizeExistingDomain) {
  const char existing[] = "test@example.com";
  EXPECT_EQ(existing, SanitizeEmail(existing));
}

TEST(GaiaAuthUtilTest, AreEmailsSame) {
  EXPECT_TRUE(AreEmailsSame("foo", "foo"));
  EXPECT_TRUE(AreEmailsSame("foo", "foo@gmail.com"));
  EXPECT_TRUE(AreEmailsSame("foo@gmail.com", "Foo@Gmail.com"));
  EXPECT_FALSE(AreEmailsSame("foo@gmail.com", "foo@othermail.com"));
  EXPECT_FALSE(AreEmailsSame("user@gmail.com", "foo@gmail.com"));
}

TEST(GaiaAuthUtilTest, IsGaiaSignonRealm) {
  // Only https versions of Gaia URLs should be considered valid.
  EXPECT_TRUE(IsGaiaSignonRealm(GURL("https://accounts.google.com/")));
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("http://accounts.google.com/")));

  // Other Google URLs are not valid.
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("https://www.google.com/")));
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("http://www.google.com/")));
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("https://google.com/")));
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("https://mail.google.com/")));

  // Other https URLs are not valid.
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("https://www.example.com/")));
}

}  // namespace gaia
