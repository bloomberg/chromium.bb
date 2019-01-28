// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kFakeHandle[] = "fake-handle";
const char kFakeSymmetricKey[] = "fake-symmetric-key";
const char kFakeSymmetricKeyBase64[] = "ZmFrZS1zeW1tZXRyaWMta2V5";
const char kFakeSymmetricKeySha256HashBase64[] =
    "+lh4oqYTenQmzyIY8XJreGDJ95A4Sk41c15BQPKOmCY=";
const char kFakePublicKey[] = "fake-public-key";
const char kFakePublicKeyBase64[] = "ZmFrZS1wdWJsaWMta2V5";
const char kFakePublicKeySha256HashBase64[] =
    "vj5oRVhZmlDrE4G4RKNV37Etgr/XuNOwEFAzb888/KM=";
const char kFakePrivateKey[] = "fake-private-key";
const char kFakePrivateKeyBase64[] = "ZmFrZS1wcml2YXRlLWtleQ==";

}  // namespace

TEST(CryptAuthKeyTest, CreateSymmetricKey) {
  CryptAuthKey key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                   cryptauthv2::KeyType::RAW256);

  ASSERT_TRUE(key.IsSymmetricKey());
  ASSERT_FALSE(key.IsAsymmetricKey());
  EXPECT_EQ(key.symmetric_key(), kFakeSymmetricKey);
  EXPECT_EQ(key.status(), CryptAuthKey::Status::kActive);
  EXPECT_EQ(key.type(), cryptauthv2::KeyType::RAW256);
  EXPECT_EQ(key.handle(), kFakeSymmetricKeySha256HashBase64);

  CryptAuthKey key_given_handle(kFakeSymmetricKey,
                                CryptAuthKey::Status::kActive,
                                cryptauthv2::KeyType::RAW256, kFakeHandle);
  EXPECT_EQ(key_given_handle.handle(), kFakeHandle);
}

TEST(CryptAuthKeyTest, CreateAsymmetricKey) {
  CryptAuthKey key(kFakePublicKey, kFakePrivateKey,
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256);

  ASSERT_FALSE(key.IsSymmetricKey());
  ASSERT_TRUE(key.IsAsymmetricKey());
  EXPECT_EQ(key.public_key(), kFakePublicKey);
  EXPECT_EQ(key.private_key(), kFakePrivateKey);
  EXPECT_EQ(key.status(), CryptAuthKey::Status::kActive);
  EXPECT_EQ(key.type(), cryptauthv2::KeyType::P256);
  EXPECT_EQ(key.handle(), kFakePublicKeySha256HashBase64);

  CryptAuthKey key_given_handle(kFakePublicKey, kFakePrivateKey,
                                CryptAuthKey::Status::kActive,
                                cryptauthv2::KeyType::P256, kFakeHandle);
  EXPECT_EQ(key_given_handle.handle(), kFakeHandle);
}

TEST(CryptAuthKeyTest, SymmetricKeyAsDictionary) {
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                             cryptauthv2::KeyType::RAW256, kFakeHandle);

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("handle", base::Value(kFakeHandle));
  dict.SetKey("status", base::Value(CryptAuthKey::Status::kActive));
  dict.SetKey("type", base::Value(cryptauthv2::KeyType::RAW256));
  dict.SetKey("symmetric_key", base::Value(kFakeSymmetricKeyBase64));

  EXPECT_EQ(symmetric_key.AsSymmetricKeyDictionary(), dict);
}

TEST(CryptAuthKeyTest, AsymmetricKeyAsDictionary) {
  CryptAuthKey asymmetric_key(kFakePublicKey, kFakePrivateKey,
                              CryptAuthKey::Status::kActive,
                              cryptauthv2::KeyType::P256, kFakeHandle);

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("handle", base::Value(kFakeHandle));
  dict.SetKey("status", base::Value(CryptAuthKey::Status::kActive));
  dict.SetKey("type", base::Value(cryptauthv2::KeyType::P256));
  dict.SetKey("public_key", base::Value(kFakePublicKeyBase64));
  dict.SetKey("private_key", base::Value(kFakePrivateKeyBase64));

  EXPECT_EQ(asymmetric_key.AsAsymmetricKeyDictionary(), dict);
}

TEST(CryptAuthKeyTest, SymmetricKeyFromDictionary) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("handle", base::Value(kFakeHandle));
  dict.SetKey("status", base::Value(CryptAuthKey::Status::kActive));
  dict.SetKey("type", base::Value(cryptauthv2::KeyType::RAW256));
  dict.SetKey("symmetric_key", base::Value(kFakeSymmetricKeyBase64));

  base::Optional<CryptAuthKey> key = CryptAuthKey::FromDictionary(dict);
  ASSERT_TRUE(key);
  EXPECT_EQ(*key, CryptAuthKey(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                               cryptauthv2::KeyType::RAW256, kFakeHandle));
}

TEST(CryptAuthKeyTest, AsymmetricKeyFromDictionary) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("handle", base::Value(kFakeHandle));
  dict.SetKey("status", base::Value(CryptAuthKey::Status::kActive));
  dict.SetKey("type", base::Value(cryptauthv2::KeyType::P256));
  dict.SetKey("public_key", base::Value(kFakePublicKeyBase64));
  dict.SetKey("private_key", base::Value(kFakePrivateKeyBase64));

  base::Optional<CryptAuthKey> key = CryptAuthKey::FromDictionary(dict);
  ASSERT_TRUE(key);
  EXPECT_EQ(*key, CryptAuthKey(kFakePublicKey, kFakePrivateKey,
                               CryptAuthKey::Status::kActive,
                               cryptauthv2::KeyType::P256, kFakeHandle));
}

TEST(CryptAuthKeyTest, KeyFromDictionary_MissingHandle) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("status", base::Value(CryptAuthKey::Status::kActive));
  dict.SetKey("type", base::Value(cryptauthv2::KeyType::RAW256));
  dict.SetKey("symmetric_key", base::Value(kFakeSymmetricKey));

  EXPECT_FALSE(CryptAuthKey::FromDictionary(dict));
}

TEST(CryptAuthKeyTest, KeyFromDictionary_MissingStatus) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("handle", base::Value(kFakeHandle));
  dict.SetKey("type", base::Value(cryptauthv2::KeyType::RAW256));
  dict.SetKey("symmetric_key", base::Value(kFakeSymmetricKey));

  EXPECT_FALSE(CryptAuthKey::FromDictionary(dict));
}

TEST(CryptAuthKeyTest, KeyFromDictionary_MissingType) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("handle", base::Value(kFakeHandle));
  dict.SetKey("status", base::Value(CryptAuthKey::Status::kActive));
  dict.SetKey("symmetric_key", base::Value(kFakeSymmetricKey));

  EXPECT_FALSE(CryptAuthKey::FromDictionary(dict));
}

TEST(CryptAuthKeyTest, SymmetricKeyFromDictionary_MissingSymmetricKey) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("handle", base::Value(kFakeHandle));
  dict.SetKey("status", base::Value(CryptAuthKey::Status::kActive));
  dict.SetKey("type", base::Value(cryptauthv2::KeyType::RAW256));

  EXPECT_FALSE(CryptAuthKey::FromDictionary(dict));
}

TEST(CryptAuthKeyTest, AsymmetricKeyFromDictionary_MissingPublicKey) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("handle", base::Value(kFakeHandle));
  dict.SetKey("status", base::Value(CryptAuthKey::Status::kActive));
  dict.SetKey("type", base::Value(cryptauthv2::KeyType::P256));
  dict.SetKey("private_key", base::Value(kFakePrivateKey));

  EXPECT_FALSE(CryptAuthKey::FromDictionary(dict));
}

TEST(CryptAuthKeyTest, AsymmetricKeyFromDictionary_MissingPrivateKey) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("handle", base::Value(kFakeHandle));
  dict.SetKey("status", base::Value(CryptAuthKey::Status::kActive));
  dict.SetKey("type", base::Value(cryptauthv2::KeyType::P256));
  dict.SetKey("public_key", base::Value(kFakePublicKey));

  EXPECT_FALSE(CryptAuthKey::FromDictionary(dict));
}

TEST(CryptAuthKeyTest, Equality) {
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                             cryptauthv2::KeyType::RAW256);
  CryptAuthKey asymmetric_key(kFakePublicKey, kFakePrivateKey,
                              CryptAuthKey::Status::kActive,
                              cryptauthv2::KeyType::P256);

  EXPECT_EQ(symmetric_key,
            CryptAuthKey(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                         cryptauthv2::KeyType::RAW256));
  EXPECT_EQ(asymmetric_key, CryptAuthKey(kFakePublicKey, kFakePrivateKey,
                                         CryptAuthKey::Status::kActive,
                                         cryptauthv2::KeyType::P256));
}

TEST(CryptAuthKeyTest, NotEquality) {
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                             cryptauthv2::KeyType::RAW256);
  CryptAuthKey asymmetric_key(kFakePublicKey, kFakePrivateKey,
                              CryptAuthKey::Status::kActive,
                              cryptauthv2::KeyType::P256);
  EXPECT_NE(symmetric_key, asymmetric_key);

  EXPECT_NE(symmetric_key,
            CryptAuthKey(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                         cryptauthv2::KeyType::RAW256));
  EXPECT_NE(symmetric_key,
            CryptAuthKey(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                         cryptauthv2::KeyType::RAW128));
  EXPECT_NE(symmetric_key,
            CryptAuthKey("different-sym-key", CryptAuthKey::Status::kActive,
                         cryptauthv2::KeyType::RAW256));

  EXPECT_NE(asymmetric_key, CryptAuthKey(kFakePublicKey, kFakePrivateKey,
                                         CryptAuthKey::Status::kInactive,
                                         cryptauthv2::KeyType::P256));
  EXPECT_NE(asymmetric_key, CryptAuthKey(kFakePublicKey, kFakePrivateKey,
                                         CryptAuthKey::Status::kActive,
                                         cryptauthv2::KeyType::CURVE25519));
  EXPECT_NE(asymmetric_key, CryptAuthKey("different-pub-key", kFakePrivateKey,
                                         CryptAuthKey::Status::kActive,
                                         cryptauthv2::KeyType::P256));
  EXPECT_NE(asymmetric_key, CryptAuthKey(kFakePublicKey, "different-priv-key",
                                         CryptAuthKey::Status::kActive,
                                         cryptauthv2::KeyType::P256));
}

}  // namespace device_sync

}  // namespace chromeos
