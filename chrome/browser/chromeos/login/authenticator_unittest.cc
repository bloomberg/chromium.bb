// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/authenticator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(AuthenticatorTest, EmailAddressNoOp) {
  const char lower_case[] = "user@what.com";
  EXPECT_EQ(lower_case, Authenticator::Canonicalize(lower_case));
}

TEST(AuthenticatorTest, EmailAddressIgnoreCaps) {
  EXPECT_EQ(Authenticator::Canonicalize("user@what.com"),
            Authenticator::Canonicalize("UsEr@what.com"));
}

TEST(AuthenticatorTest, EmailAddressIgnoreDomainCaps) {
  EXPECT_EQ(Authenticator::Canonicalize("user@what.com"),
            Authenticator::Canonicalize("UsEr@what.COM"));
}

TEST(AuthenticatorTest, EmailAddressRejectOneUsernameDot) {
  EXPECT_NE(Authenticator::Canonicalize("u.ser@what.com"),
            Authenticator::Canonicalize("UsEr@what.com"));
}

TEST(AuthenticatorTest, EmailAddressMatchWithOneUsernameDot) {
  EXPECT_EQ(Authenticator::Canonicalize("u.ser@what.com"),
            Authenticator::Canonicalize("U.sEr@what.com"));
}

TEST(AuthenticatorTest, EmailAddressIgnoreOneUsernameDot) {
  EXPECT_EQ(Authenticator::Canonicalize("us.er@gmail.com"),
            Authenticator::Canonicalize("UsEr@gmail.com"));
}

TEST(AuthenticatorTest, EmailAddressIgnoreManyUsernameDots) {
  EXPECT_EQ(Authenticator::Canonicalize("u.ser@gmail.com"),
            Authenticator::Canonicalize("Us.E.r@gmail.com"));
}

TEST(AuthenticatorTest, EmailAddressIgnoreConsecutiveUsernameDots) {
  EXPECT_EQ(Authenticator::Canonicalize("use.r@gmail.com"),
            Authenticator::Canonicalize("Us....E.r@gmail.com"));
}

TEST(AuthenticatorTest, EmailAddressDifferentOnesRejected) {
  EXPECT_NE(Authenticator::Canonicalize("who@what.com"),
            Authenticator::Canonicalize("Us....E.r@what.com"));
}

TEST(AuthenticatorTest, EmailAddressIgnorePlusSuffix) {
  const char with_plus[] = "user+cc@what.com";
  EXPECT_EQ(with_plus, Authenticator::Canonicalize(with_plus));
}

TEST(AuthenticatorTest, EmailAddressIgnoreMultiPlusSuffix) {
  const char multi_plus[] = "user+cc+bcc@what.com";
  EXPECT_EQ(multi_plus, Authenticator::Canonicalize(multi_plus));
}

}  // namespace chromeos
