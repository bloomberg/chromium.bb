// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_passphrase.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

// This test checks the class ManagedUserPassphrase when providing a salt
// as parameter of the constructor.
TEST(ManagedUserPassphraseTest, WithSaltProvided) {
  std::string salt = "my special salt";
  std::string salt2 = "my other special salt";
  ManagedUserPassphrase instance_with_provided_salt(salt);
  ManagedUserPassphrase other_instance_with_provided_salt(salt2);

  // We expect that the provided salt is used internally as well.
  EXPECT_STREQ(salt.c_str(), instance_with_provided_salt.GetSalt().c_str());
  std::string passphrase_hash;
  std::string passphrase = "some_passphrase123";
  EXPECT_TRUE(instance_with_provided_salt.GenerateHashFromPassphrase(
      passphrase,
      &passphrase_hash));
  // As the method generates a Base64 encoded 128 bit key, we expect the
  // passphrase hash to have length at least 7 bytes.
  EXPECT_GE(passphrase_hash.size(), 7u);

  // When calling the function with a (slightly) different parameter, we
  // expect to get a different result.
  std::string passphrase2 = passphrase + "4";
  std::string passphrase_hash2;
  EXPECT_TRUE(instance_with_provided_salt.GenerateHashFromPassphrase(
      passphrase2,
      &passphrase_hash2));
  EXPECT_GE(passphrase_hash2.size(), 7u);
  EXPECT_STRNE(passphrase_hash.c_str(), passphrase_hash2.c_str());

  // When calling the function again with the first parameter, we expect to
  // get the same result as in the first call.
  EXPECT_TRUE(instance_with_provided_salt.GenerateHashFromPassphrase(
      passphrase,
      &passphrase_hash2));
  EXPECT_STREQ(passphrase_hash.c_str(), passphrase_hash2.c_str());

  // When calling the function on the instance with the other salt, but
  // with the same passphrase, we expect to get a different result.
  EXPECT_TRUE(other_instance_with_provided_salt.GenerateHashFromPassphrase(
      passphrase,
      &passphrase_hash2));
  EXPECT_STRNE(passphrase_hash.c_str(), passphrase_hash2.c_str());
}

// This test checks the class ManagedUserPassphraseTest when no salt is
// provided as parameter of the constructor.
TEST(ManagedUserPassphraseTest, WithEmptySalt) {
  ManagedUserPassphrase instance_with_empty_salt((std::string()));
  ManagedUserPassphrase other_instance_with_empty_salt((std::string()));
  std::string salt = instance_with_empty_salt.GetSalt();
  std::string salt2 = other_instance_with_empty_salt.GetSalt();

  // We expect that the class will generate a salt randomly, and for different
  // instances a different salt is calculated.
  EXPECT_GT(salt.size(), 0u);
  EXPECT_GT(salt2.size(), 0u);
  EXPECT_STRNE(salt.c_str(), salt2.c_str());
}
