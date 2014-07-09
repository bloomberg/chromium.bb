// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/key.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kPassword[] = "password";
const char kLabel[] = "label";
const char kSalt[] =
    "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";

}  // namespace

TEST(KeyTest, ClearSecret) {
  Key key(kPassword);
  key.SetLabel(kLabel);
  EXPECT_EQ(Key::KEY_TYPE_PASSWORD_PLAIN, key.GetKeyType());
  EXPECT_EQ(kPassword, key.GetSecret());
  EXPECT_EQ(kLabel, key.GetLabel());

  key.ClearSecret();
  EXPECT_EQ(Key::KEY_TYPE_PASSWORD_PLAIN, key.GetKeyType());
  EXPECT_TRUE(key.GetSecret().empty());
  EXPECT_EQ(kLabel, key.GetLabel());
}

TEST(KeyTest, TransformToSaltedSHA256TopHalf) {
  Key key(kPassword);
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, kSalt);
  EXPECT_EQ(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, key.GetKeyType());
  EXPECT_EQ("5b01941771e47fa408380aa675703f4f", key.GetSecret());
}

TEST(KeyTest, TransformToSaltedAES2561234) {
  Key key(kPassword);
  key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, kSalt);
  EXPECT_EQ(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, key.GetKeyType());
  EXPECT_EQ("GUkNnvqoULf/cXbZscVUnANmLBB0ovjGZsj1sKzP5BE=", key.GetSecret());
}

}  // namespace chromeos
