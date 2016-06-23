// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/origin_trials/chrome_origin_trial_policy.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

const uint8_t kTestPublicKey[] = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

// Base64 encoding of the above sample public key
const char kTestPublicKeyString[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";
const char kBadEncodingPublicKeyString[] = "Not even base64!";
// Base64-encoded, 31 bytes long
const char kTooShortPublicKeyString[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BN==";
// Base64-encoded, 33 bytes long
const char kTooLongPublicKeyString[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNAA";

const char kOneDisabledFeature[] = "A";
const char kTwoDisabledFeatures[] = "A|B";
const char kThreeDisabledFeatures[] = "A|B|C";
const char kSpacesInDisabledFeatures[] = "A|B C";

class ChromeOriginTrialPolicyTest : public testing::Test {
 protected:
  ChromeOriginTrialPolicyTest()
      : manager_(base::WrapUnique(new ChromeOriginTrialPolicy())),
        default_key_(manager_->GetPublicKey().as_string()),
        test_key_(std::string(reinterpret_cast<const char*>(kTestPublicKey),
                              arraysize(kTestPublicKey))) {}
  ChromeOriginTrialPolicy* manager() { return manager_.get(); }
  base::StringPiece default_key() { return default_key_; }
  base::StringPiece test_key() { return test_key_; }

 private:
  std::unique_ptr<ChromeOriginTrialPolicy> manager_;
  std::string default_key_;
  std::string test_key_;
};

TEST_F(ChromeOriginTrialPolicyTest, DefaultConstructor) {
  // We don't specify here what the key should be, but make sure that it is
  // returned, is valid, and is consistent.
  base::StringPiece key = manager()->GetPublicKey();
  EXPECT_EQ(32UL, key.size());
  EXPECT_EQ(default_key(), key);
}

TEST_F(ChromeOriginTrialPolicyTest, DefaultKeyIsConsistent) {
  ChromeOriginTrialPolicy manager2;
  EXPECT_EQ(manager()->GetPublicKey(), manager2.GetPublicKey());
}

TEST_F(ChromeOriginTrialPolicyTest, OverridePublicKey) {
  EXPECT_TRUE(manager()->SetPublicKeyFromASCIIString(kTestPublicKeyString));
  EXPECT_NE(default_key(), manager()->GetPublicKey());
  EXPECT_EQ(test_key(), manager()->GetPublicKey());
}

TEST_F(ChromeOriginTrialPolicyTest, OverrideKeyNotBase64) {
  EXPECT_FALSE(
      manager()->SetPublicKeyFromASCIIString(kBadEncodingPublicKeyString));
  EXPECT_EQ(default_key(), manager()->GetPublicKey());
}

TEST_F(ChromeOriginTrialPolicyTest, OverrideKeyTooShort) {
  EXPECT_FALSE(
      manager()->SetPublicKeyFromASCIIString(kTooShortPublicKeyString));
  EXPECT_EQ(default_key(), manager()->GetPublicKey());
}

TEST_F(ChromeOriginTrialPolicyTest, OverrideKeyTooLong) {
  EXPECT_FALSE(manager()->SetPublicKeyFromASCIIString(kTooLongPublicKeyString));
  EXPECT_EQ(default_key(), manager()->GetPublicKey());
}

TEST_F(ChromeOriginTrialPolicyTest, NoDisabledFeatures) {
  EXPECT_FALSE(manager()->IsFeatureDisabled("A"));
  EXPECT_FALSE(manager()->IsFeatureDisabled("B"));
  EXPECT_FALSE(manager()->IsFeatureDisabled("C"));
}

TEST_F(ChromeOriginTrialPolicyTest, DisableOneFeature) {
  EXPECT_TRUE(manager()->SetDisabledFeatures(kOneDisabledFeature));
  EXPECT_TRUE(manager()->IsFeatureDisabled("A"));
  EXPECT_FALSE(manager()->IsFeatureDisabled("B"));
}

TEST_F(ChromeOriginTrialPolicyTest, DisableTwoFeatures) {
  EXPECT_TRUE(manager()->SetDisabledFeatures(kTwoDisabledFeatures));
  EXPECT_TRUE(manager()->IsFeatureDisabled("A"));
  EXPECT_TRUE(manager()->IsFeatureDisabled("B"));
  EXPECT_FALSE(manager()->IsFeatureDisabled("C"));
}

TEST_F(ChromeOriginTrialPolicyTest, DisableThreeFeatures) {
  EXPECT_TRUE(manager()->SetDisabledFeatures(kThreeDisabledFeatures));
  EXPECT_TRUE(manager()->IsFeatureDisabled("A"));
  EXPECT_TRUE(manager()->IsFeatureDisabled("B"));
  EXPECT_TRUE(manager()->IsFeatureDisabled("C"));
}

TEST_F(ChromeOriginTrialPolicyTest, DisableFeatureWithSpace) {
  EXPECT_TRUE(manager()->SetDisabledFeatures(kSpacesInDisabledFeatures));
  EXPECT_TRUE(manager()->IsFeatureDisabled("A"));
  EXPECT_TRUE(manager()->IsFeatureDisabled("B C"));
  EXPECT_FALSE(manager()->IsFeatureDisabled("B"));
  EXPECT_FALSE(manager()->IsFeatureDisabled("C"));
}
