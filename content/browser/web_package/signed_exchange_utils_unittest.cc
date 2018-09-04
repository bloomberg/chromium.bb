// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace signed_exchange_utils {

TEST(SignedExchangeUtilsTest, VersionParam_None) {
  base::Optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange");
  EXPECT_FALSE(version.has_value());
}

TEST(SignedExchangeUtilsTest, VersionParam_NoneWithSemicolon) {
  base::Optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;");
  EXPECT_FALSE(version.has_value());
}

TEST(SignedExchangeUtilsTest, VersionParam_Empty) {
  base::Optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=");
  EXPECT_FALSE(version.has_value());
}

TEST(SignedExchangeUtilsTest, VersionParam_EmptyString) {
  base::Optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"\"");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_UnknownVersion) {
  base::Optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=foobar");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_Simple) {
  base::Optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=b2");
  EXPECT_EQ(version, SignedExchangeVersion::kB2);
}

TEST(SignedExchangeUtilsTest, VersionParam_WithSpace) {
  base::Optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange ; v=b2");
  EXPECT_EQ(version, SignedExchangeVersion::kB2);
}

TEST(SignedExchangeUtilsTest, VersionParam_ExtraParam) {
  base::Optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=b2;foo=bar");
  EXPECT_EQ(version, SignedExchangeVersion::kB2);
}

TEST(SignedExchangeUtilsTest, VersionParam_Quoted) {
  base::Optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"b2\"");
  EXPECT_EQ(version, SignedExchangeVersion::kB2);
}

TEST(SignedExchangeUtilsTest, VersionParam_UseCaseInsensitiveMatch) {
  base::Optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("Application/Signed-Exchange;V=b2");
  EXPECT_EQ(version, SignedExchangeVersion::kB2);
}

}  // namespace signed_exchange_utils
}  // namespace content
