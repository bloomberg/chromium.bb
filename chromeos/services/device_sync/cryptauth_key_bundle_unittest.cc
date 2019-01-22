// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kFakeSymmetricKeyHandle[] = "fake-symmetric-key-handle";
const char kFakeAsymmetricKeyHandle[] = "fake-asymmetric-key-handle";
const char kFakeSymmetricKey[] = "fake-symmetric-key";
const char kFakePublicKey[] = "fake-public-key";
const char kFakePrivateKey[] = "fake-private-key";

}  // namespace

TEST(CryptAuthKeyBundleTest, CreateKeyBundle) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  EXPECT_EQ(bundle.name(), CryptAuthKeyBundle::Name::kUserKeyPair);
  EXPECT_TRUE(bundle.handle_to_key_map().empty());
  EXPECT_FALSE(bundle.key_directive());
}

TEST(CryptAuthKeyBundleTest, SetKeyDirective) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  cryptauthv2::KeyDirective key_directive;
  bundle.set_key_directive(key_directive);
  ASSERT_TRUE(bundle.key_directive());
  EXPECT_EQ(bundle.key_directive()->SerializeAsString(),
            key_directive.SerializeAsString());
}

TEST(CryptAuthKeyBundleTest, AddKey) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  CryptAuthKey key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                   cryptauthv2::KeyType::RAW256, kFakeSymmetricKeyHandle);
  bundle.AddKey(key);
  EXPECT_TRUE(bundle.handle_to_key_map().size() == 1);

  const auto& it = bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle);
  ASSERT_TRUE(it != bundle.handle_to_key_map().end());
  EXPECT_EQ(it->first, key.handle());
  EXPECT_EQ(it->second, key);
}

TEST(CryptAuthKeyBundleTest, AddKey_Inactive) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  EXPECT_EQ(
      bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle)->second.status(),
      CryptAuthKey::Status::kActive);

  CryptAuthKey asymmetric_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kInactive,
      cryptauthv2::KeyType::P256, kFakeAsymmetricKeyHandle);
  bundle.AddKey(asymmetric_key);
  EXPECT_EQ(
      bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle)->second.status(),
      CryptAuthKey::Status::kActive);
  EXPECT_EQ(bundle.handle_to_key_map()
                .find(kFakeAsymmetricKeyHandle)
                ->second.status(),
            CryptAuthKey::Status::kInactive);
}

TEST(CryptAuthKeyBundleTest, AddKey_ActiveKeyDeactivatesOthers) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  EXPECT_EQ(
      bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle)->second.status(),
      CryptAuthKey::Status::kActive);

  CryptAuthKey asymmetric_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kFakeAsymmetricKeyHandle);
  bundle.AddKey(asymmetric_key);
  EXPECT_EQ(
      bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle)->second.status(),
      CryptAuthKey::Status::kInactive);
  EXPECT_EQ(bundle.handle_to_key_map()
                .find(kFakeAsymmetricKeyHandle)
                ->second.status(),
            CryptAuthKey::Status::kActive);
}

TEST(CryptAuthKeyBundleTest, AddKey_ReplaceKeyWithSameHandle) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                             cryptauthv2::KeyType::RAW256, "same-handle");
  bundle.AddKey(symmetric_key);
  EXPECT_EQ(bundle.handle_to_key_map().find("same-handle")->second,
            symmetric_key);
  CryptAuthKey asymmetric_key(kFakePublicKey, kFakePrivateKey,
                              CryptAuthKey::Status::kActive,
                              cryptauthv2::KeyType::P256, "same-handle");
  bundle.AddKey(asymmetric_key);
  EXPECT_TRUE(bundle.handle_to_key_map().size() == 1);
  EXPECT_EQ(bundle.handle_to_key_map().find("same-handle")->second,
            asymmetric_key);
}

TEST(CryptAuthKeyBundleTest, GetActiveKey_DoesNotExist) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  EXPECT_FALSE(bundle.GetActiveKey());

  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  EXPECT_FALSE(bundle.GetActiveKey());
}

TEST(CryptAuthKeyBundleTest, GetActiveKey_Exists) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  CryptAuthKey asymmetric_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kFakeAsymmetricKeyHandle);
  bundle.AddKey(asymmetric_key);

  ASSERT_TRUE(bundle.GetActiveKey());
  EXPECT_EQ(*bundle.GetActiveKey(), asymmetric_key);
}

TEST(CryptAuthKeyBundleTest, SetActiveKey_InactiveToActive) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  CryptAuthKey asymmetric_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kFakeAsymmetricKeyHandle);
  bundle.AddKey(asymmetric_key);

  bundle.SetActiveKey(kFakeSymmetricKeyHandle);

  EXPECT_EQ(
      bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle)->second.status(),
      CryptAuthKey::Status::kActive);
  EXPECT_EQ(bundle.handle_to_key_map()
                .find(kFakeAsymmetricKeyHandle)
                ->second.status(),
            CryptAuthKey::Status::kInactive);
}

TEST(CryptAuthKeyBundleTest, SetActiveKey_ActiveToActive) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  CryptAuthKey asymmetric_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kFakeAsymmetricKeyHandle);
  bundle.AddKey(asymmetric_key);

  bundle.SetActiveKey(kFakeAsymmetricKeyHandle);

  EXPECT_EQ(
      bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle)->second.status(),
      CryptAuthKey::Status::kInactive);
  EXPECT_EQ(bundle.handle_to_key_map()
                .find(kFakeAsymmetricKeyHandle)
                ->second.status(),
            CryptAuthKey::Status::kActive);
}

TEST(CryptAuthKeyBundleTest, DeactivateKeys) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  CryptAuthKey asymmetric_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kFakeAsymmetricKeyHandle);
  bundle.AddKey(asymmetric_key);

  bundle.DeactivateKeys();

  EXPECT_EQ(
      bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle)->second.status(),
      CryptAuthKey::Status::kInactive);
  EXPECT_EQ(bundle.handle_to_key_map()
                .find(kFakeAsymmetricKeyHandle)
                ->second.status(),
            CryptAuthKey::Status::kInactive);
}

TEST(CryptAuthKeyBundleTest, DeleteKey) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);

  EXPECT_TRUE(bundle.handle_to_key_map().size() == 1);
  bundle.DeleteKey(kFakeSymmetricKeyHandle);
  EXPECT_TRUE(bundle.handle_to_key_map().empty());
}

TEST(CryptAuthKeyBundleTest, ToAndFromDictionary_Trivial) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  base::Optional<CryptAuthKeyBundle> bundle_from_dict =
      CryptAuthKeyBundle::FromDictionary(bundle.AsDictionary());
  ASSERT_TRUE(bundle_from_dict);
  EXPECT_EQ(*bundle_from_dict, bundle);
}

TEST(CryptAuthKeyBundleTest, ToAndFromDictionary) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  CryptAuthKey asymmetric_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kFakeAsymmetricKeyHandle);
  bundle.AddKey(asymmetric_key);

  cryptauthv2::KeyDirective key_directive;
  key_directive.mutable_policy_reference()->set_name("fake-policy-name");
  key_directive.mutable_policy_reference()->set_version(42);
  *key_directive.add_crossproof_key_names() = "fake-key-name-1";
  *key_directive.add_crossproof_key_names() = "fake-key-name-2";
  key_directive.set_enroll_time_millis(1000);
  bundle.set_key_directive(key_directive);

  base::Optional<CryptAuthKeyBundle> bundle_from_dict =
      CryptAuthKeyBundle::FromDictionary(bundle.AsDictionary());
  ASSERT_TRUE(bundle_from_dict);
  EXPECT_EQ(*bundle_from_dict, bundle);
}

}  // namespace device_sync

}  // namespace chromeos
