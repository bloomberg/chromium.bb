// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key_registry_impl.h"

#include "base/stl_util.h"
#include "chromeos/services/device_sync/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

class CryptAuthKeyRegistryImplTest : public testing::Test {
 protected:
  CryptAuthKeyRegistryImplTest() = default;

  ~CryptAuthKeyRegistryImplTest() override = default;

  void SetUp() override {
    CryptAuthKeyRegistryImpl::RegisterPrefs(pref_service_.registry());
    key_registry_ =
        CryptAuthKeyRegistryImpl::Factory::Get()->BuildInstance(&pref_service_);
  }

  // Verify that changing the in-memory key bundle map updates the pref.
  void VerifyPrefValue(const base::Value& expected_dict) {
    const base::DictionaryValue* dict =
        pref_service_.GetDictionary(prefs::kCryptAuthKeyRegistry);
    ASSERT_TRUE(dict);
    EXPECT_EQ(*dict, expected_dict);
  }

  PrefService* pref_service() { return &pref_service_; }

  CryptAuthKeyRegistry* key_registry() { return key_registry_.get(); }

 private:
  TestingPrefServiceSimple pref_service_;

  std::unique_ptr<CryptAuthKeyRegistry> key_registry_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthKeyRegistryImplTest);
};

TEST_F(CryptAuthKeyRegistryImplTest, GetActiveKey_NoActiveKey) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kInactive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  key_registry()->AddEnrolledKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                                 sym_key);

  EXPECT_FALSE(
      key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair));
}

TEST_F(CryptAuthKeyRegistryImplTest, GetActiveKey) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kInactive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  CryptAuthKey asym_key("public-key", "private-key",
                        CryptAuthKey::Status::kActive,
                        cryptauthv2::KeyType::P256, "asym-handle");
  key_registry()->AddEnrolledKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                                 sym_key);
  key_registry()->AddEnrolledKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                                 asym_key);

  const CryptAuthKey* key =
      key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair);
  ASSERT_TRUE(key);
  EXPECT_EQ(*key, asym_key);
}

TEST_F(CryptAuthKeyRegistryImplTest, AddKey) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kActive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  key_registry()->AddEnrolledKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                                 sym_key);
  const auto& it = key_registry()->enrolled_key_bundles().find(
      CryptAuthKeyBundle::Name::kUserKeyPair);
  ASSERT_TRUE(it != key_registry()->enrolled_key_bundles().end());

  const CryptAuthKey* active_key =
      key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair);
  ASSERT_TRUE(active_key);
  EXPECT_EQ(*active_key, sym_key);

  CryptAuthKeyBundle expected_bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  expected_bundle.AddKey(sym_key);
  EXPECT_EQ(expected_bundle, it->second);

  base::Value expected_dict(base::Value::Type::DICTIONARY);
  expected_dict.SetKey(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(expected_bundle.name()),
      expected_bundle.AsDictionary());
  VerifyPrefValue(expected_dict);

  // Add another key to same bundle
  CryptAuthKey asym_key("public-key", "private-key",
                        CryptAuthKey::Status::kActive,
                        cryptauthv2::KeyType::P256, "asym-handle");
  key_registry()->AddEnrolledKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                                 asym_key);

  expected_bundle.AddKey(asym_key);
  EXPECT_EQ(expected_bundle, it->second);

  active_key =
      key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair);
  ASSERT_TRUE(active_key);
  EXPECT_EQ(*active_key, asym_key);

  expected_dict.SetKey(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(expected_bundle.name()),
      expected_bundle.AsDictionary());
  VerifyPrefValue(expected_dict);
}

TEST_F(CryptAuthKeyRegistryImplTest, SetActiveKey) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kInactive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  CryptAuthKey asym_key("public-key", "private-key",
                        CryptAuthKey::Status::kActive,
                        cryptauthv2::KeyType::P256, "asym-handle");
  key_registry()->AddEnrolledKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                                 sym_key);
  key_registry()->AddEnrolledKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                                 asym_key);

  key_registry()->SetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                               "sym-handle");

  const CryptAuthKey* key =
      key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair);
  EXPECT_TRUE(key);

  sym_key.set_status(CryptAuthKey::Status::kActive);
  EXPECT_EQ(*key, sym_key);

  CryptAuthKeyBundle expected_bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  expected_bundle.AddKey(sym_key);
  asym_key.set_status(CryptAuthKey::Status::kInactive);
  expected_bundle.AddKey(asym_key);
  base::Value expected_dict(base::Value::Type::DICTIONARY);
  expected_dict.SetKey(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(expected_bundle.name()),
      expected_bundle.AsDictionary());
  VerifyPrefValue(expected_dict);
}

TEST_F(CryptAuthKeyRegistryImplTest, DeactivateKeys) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kInactive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  CryptAuthKey asym_key("public-key", "private-key",
                        CryptAuthKey::Status::kActive,
                        cryptauthv2::KeyType::P256, "asym-handle");
  key_registry()->AddEnrolledKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                                 sym_key);
  key_registry()->AddEnrolledKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                                 asym_key);

  key_registry()->DeactivateKeys(CryptAuthKeyBundle::Name::kUserKeyPair);

  EXPECT_FALSE(
      key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair));

  CryptAuthKeyBundle expected_bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  expected_bundle.AddKey(sym_key);
  asym_key.set_status(CryptAuthKey::Status::kInactive);
  expected_bundle.AddKey(asym_key);
  base::Value expected_dict(base::Value::Type::DICTIONARY);
  expected_dict.SetKey(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(expected_bundle.name()),
      expected_bundle.AsDictionary());
  VerifyPrefValue(expected_dict);
}

TEST_F(CryptAuthKeyRegistryImplTest, DeleteKey) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kInactive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  CryptAuthKey asym_key("public-key", "private-key",
                        CryptAuthKey::Status::kActive,
                        cryptauthv2::KeyType::P256, "asym-handle");
  key_registry()->AddEnrolledKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                                 sym_key);
  key_registry()->AddEnrolledKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                                 asym_key);

  key_registry()->DeleteKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                            "sym-handle");

  const auto& it = key_registry()->enrolled_key_bundles().find(
      CryptAuthKeyBundle::Name::kUserKeyPair);
  ASSERT_TRUE(it != key_registry()->enrolled_key_bundles().end());

  EXPECT_FALSE(base::ContainsKey(it->second.handle_to_key_map(), "sym-handle"));
  EXPECT_TRUE(base::ContainsKey(it->second.handle_to_key_map(), "asym-handle"));

  CryptAuthKeyBundle expected_bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  expected_bundle.AddKey(asym_key);
  base::Value expected_dict(base::Value::Type::DICTIONARY);
  expected_dict.SetKey(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(expected_bundle.name()),
      expected_bundle.AsDictionary());
  VerifyPrefValue(expected_dict);
}

TEST_F(CryptAuthKeyRegistryImplTest, SetKeyDirective) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kInactive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  key_registry()->AddEnrolledKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                                 sym_key);

  cryptauthv2::KeyDirective key_directive;
  key_directive.set_enroll_time_millis(1000);
  key_registry()->SetKeyDirective(CryptAuthKeyBundle::Name::kUserKeyPair,
                                  key_directive);

  const auto& it = key_registry()->enrolled_key_bundles().find(
      CryptAuthKeyBundle::Name::kUserKeyPair);
  ASSERT_TRUE(it != key_registry()->enrolled_key_bundles().end());

  EXPECT_TRUE(it->second.key_directive());
  EXPECT_EQ(it->second.key_directive()->SerializeAsString(),
            key_directive.SerializeAsString());

  CryptAuthKeyBundle expected_bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  expected_bundle.AddKey(sym_key);
  expected_bundle.set_key_directive(key_directive);
  base::Value expected_dict(base::Value::Type::DICTIONARY);
  expected_dict.SetKey(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(expected_bundle.name()),
      expected_bundle.AsDictionary());
  VerifyPrefValue(expected_dict);
}

TEST_F(CryptAuthKeyRegistryImplTest, ConstructorPopulatesBundlesUsingPref) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kInactive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  key_registry()->AddEnrolledKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                                 sym_key);
  cryptauthv2::KeyDirective key_directive;
  key_directive.set_enroll_time_millis(1000);
  key_registry()->SetKeyDirective(CryptAuthKeyBundle::Name::kUserKeyPair,
                                  key_directive);

  // A new registry using the same pref service that was just written.
  std::unique_ptr<CryptAuthKeyRegistry> new_registry =
      CryptAuthKeyRegistryImpl::Factory::Get()->BuildInstance(pref_service());

  EXPECT_TRUE(new_registry->enrolled_key_bundles().size() == 1);

  const auto& it = new_registry->enrolled_key_bundles().find(
      CryptAuthKeyBundle::Name::kUserKeyPair);
  ASSERT_TRUE(it != new_registry->enrolled_key_bundles().end());

  CryptAuthKeyBundle expected_bundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  expected_bundle.AddKey(sym_key);
  expected_bundle.set_key_directive(key_directive);
  EXPECT_EQ(it->second, expected_bundle);
}

}  // namespace device_sync

}  // namespace chromeos
