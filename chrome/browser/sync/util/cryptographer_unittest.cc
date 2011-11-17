// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/cryptographer.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/sync/protocol/nigori_specifics.pb.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

using ::testing::Mock;
using ::testing::StrictMock;
using syncable::ModelTypeSet;

namespace {

class MockObserver : public Cryptographer::Observer {
 public:
  MOCK_METHOD2(OnEncryptedTypesChanged,
               void(const syncable::ModelTypeSet&, bool));
};

}  // namespace

TEST(CryptographerTest, EmptyCantDecrypt) {
  Cryptographer cryptographer;
  EXPECT_FALSE(cryptographer.is_ready());

  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name("foo");
  encrypted.set_blob("bar");

  EXPECT_FALSE(cryptographer.CanDecrypt(encrypted));
}

TEST(CryptographerTest, EmptyCantEncrypt) {
  Cryptographer cryptographer;
  EXPECT_FALSE(cryptographer.is_ready());

  sync_pb::EncryptedData encrypted;
  sync_pb::PasswordSpecificsData original;
  EXPECT_FALSE(cryptographer.Encrypt(original, &encrypted));
}

TEST(CryptographerTest, MissingCantDecrypt) {
  Cryptographer cryptographer;

  KeyParams params = {"localhost", "dummy", "dummy"};
  cryptographer.AddKey(params);
  EXPECT_TRUE(cryptographer.is_ready());

  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name("foo");
  encrypted.set_blob("bar");

  EXPECT_FALSE(cryptographer.CanDecrypt(encrypted));
}

TEST(CryptographerTest, CanEncryptAndDecrypt) {
  Cryptographer cryptographer;

  KeyParams params = {"localhost", "dummy", "dummy"};
  EXPECT_TRUE(cryptographer.AddKey(params));
  EXPECT_TRUE(cryptographer.is_ready());

  sync_pb::PasswordSpecificsData original;
  original.set_origin("http://example.com");
  original.set_username_value("azure");
  original.set_password_value("hunter2");

  sync_pb::EncryptedData encrypted;
  EXPECT_TRUE(cryptographer.Encrypt(original, &encrypted));

  sync_pb::PasswordSpecificsData decrypted;
  EXPECT_TRUE(cryptographer.Decrypt(encrypted, &decrypted));

  EXPECT_EQ(original.SerializeAsString(), decrypted.SerializeAsString());
}

TEST(CryptographerTest, AddKeySetsDefault) {
  Cryptographer cryptographer;

  KeyParams params1 = {"localhost", "dummy", "dummy1"};
  EXPECT_TRUE(cryptographer.AddKey(params1));
  EXPECT_TRUE(cryptographer.is_ready());

  sync_pb::PasswordSpecificsData original;
  original.set_origin("http://example.com");
  original.set_username_value("azure");
  original.set_password_value("hunter2");

  sync_pb::EncryptedData encrypted1;
  EXPECT_TRUE(cryptographer.Encrypt(original, &encrypted1));
  sync_pb::EncryptedData encrypted2;
  EXPECT_TRUE(cryptographer.Encrypt(original, &encrypted2));

  KeyParams params2 = {"localhost", "dummy", "dummy2"};
  EXPECT_TRUE(cryptographer.AddKey(params2));
  EXPECT_TRUE(cryptographer.is_ready());

  sync_pb::EncryptedData encrypted3;
  EXPECT_TRUE(cryptographer.Encrypt(original, &encrypted3));
  sync_pb::EncryptedData encrypted4;
  EXPECT_TRUE(cryptographer.Encrypt(original, &encrypted4));

  EXPECT_EQ(encrypted1.key_name(), encrypted2.key_name());
  EXPECT_NE(encrypted1.key_name(), encrypted3.key_name());
  EXPECT_EQ(encrypted3.key_name(), encrypted4.key_name());
}

// Crashes, Bug 55178.
#if defined(OS_WIN)
#define MAYBE_EncryptExportDecrypt DISABLED_EncryptExportDecrypt
#else
#define MAYBE_EncryptExportDecrypt EncryptExportDecrypt
#endif
TEST(CryptographerTest, MAYBE_EncryptExportDecrypt) {
  sync_pb::EncryptedData nigori;
  sync_pb::EncryptedData encrypted;

  sync_pb::PasswordSpecificsData original;
  original.set_origin("http://example.com");
  original.set_username_value("azure");
  original.set_password_value("hunter2");

  {
    Cryptographer cryptographer;

    KeyParams params = {"localhost", "dummy", "dummy"};
    cryptographer.AddKey(params);
    EXPECT_TRUE(cryptographer.is_ready());

    EXPECT_TRUE(cryptographer.Encrypt(original, &encrypted));
    EXPECT_TRUE(cryptographer.GetKeys(&nigori));
  }

  {
    Cryptographer cryptographer;
    EXPECT_FALSE(cryptographer.CanDecrypt(nigori));

    cryptographer.SetPendingKeys(nigori);
    EXPECT_FALSE(cryptographer.is_ready());
    EXPECT_TRUE(cryptographer.has_pending_keys());

    KeyParams params = {"localhost", "dummy", "dummy"};
    EXPECT_TRUE(cryptographer.DecryptPendingKeys(params));
    EXPECT_TRUE(cryptographer.is_ready());
    EXPECT_FALSE(cryptographer.has_pending_keys());

    sync_pb::PasswordSpecificsData decrypted;
    EXPECT_TRUE(cryptographer.Decrypt(encrypted, &decrypted));
    EXPECT_EQ(original.SerializeAsString(), decrypted.SerializeAsString());
  }
}

// Crashes, Bug 55178.
#if defined(OS_WIN)
#define MAYBE_PackUnpack DISABLED_PackUnpack
#else
#define MAYBE_PackUnpack PackUnpack
#endif
TEST(CryptographerTest, MAYBE_PackUnpack) {
#if defined(OS_MACOSX)
  Encryptor::UseMockKeychain(true);
#endif

  Nigori nigori;
  ASSERT_TRUE(nigori.InitByDerivation("example.com", "username", "password"));
  std::string expected_user, expected_encryption, expected_mac;
  ASSERT_TRUE(nigori.ExportKeys(&expected_user, &expected_encryption,
                                &expected_mac));

  Cryptographer cryptographer;
  std::string token;
  EXPECT_TRUE(cryptographer.PackBootstrapToken(&nigori, &token));
  EXPECT_TRUE(IsStringUTF8(token));

  scoped_ptr<Nigori> unpacked(cryptographer.UnpackBootstrapToken(token));
  EXPECT_NE(static_cast<Nigori*>(NULL), unpacked.get());

  std::string user_key, encryption_key, mac_key;
  ASSERT_TRUE(unpacked->ExportKeys(&user_key, &encryption_key, &mac_key));

  EXPECT_EQ(expected_user, user_key);
  EXPECT_EQ(expected_encryption, encryption_key);
  EXPECT_EQ(expected_mac, mac_key);
}

TEST(CryptographerTest, NigoriEncryptionTypes) {
  Cryptographer cryptographer;
  Cryptographer cryptographer2;
  sync_pb::NigoriSpecifics nigori;

  StrictMock<MockObserver> observer;
  cryptographer.AddObserver(&observer);
  StrictMock<MockObserver> observer2;
  cryptographer2.AddObserver(&observer2);

  // Just set the sensitive types (shouldn't trigger any
  // notifications).
  ModelTypeSet encrypted_types(Cryptographer::SensitiveTypes());
  cryptographer.MergeEncryptedTypesForTest(encrypted_types);
  cryptographer.UpdateNigoriFromEncryptedTypes(&nigori);
  cryptographer2.UpdateEncryptedTypesFromNigori(nigori);
  EXPECT_EQ(encrypted_types, cryptographer.GetEncryptedTypes());
  EXPECT_EQ(encrypted_types, cryptographer2.GetEncryptedTypes());

  Mock::VerifyAndClearExpectations(&observer);
  Mock::VerifyAndClearExpectations(&observer2);

  EXPECT_CALL(observer,
              OnEncryptedTypesChanged(syncable::GetAllRealModelTypes(),
                                      false));
  EXPECT_CALL(observer2,
              OnEncryptedTypesChanged(syncable::GetAllRealModelTypes(),
                                      false));

  // Set all encrypted types
  encrypted_types = syncable::GetAllRealModelTypes();
  cryptographer.MergeEncryptedTypesForTest(encrypted_types);
  cryptographer.UpdateNigoriFromEncryptedTypes(&nigori);
  cryptographer2.UpdateEncryptedTypesFromNigori(nigori);
  EXPECT_EQ(encrypted_types, cryptographer.GetEncryptedTypes());
  EXPECT_EQ(encrypted_types, cryptographer2.GetEncryptedTypes());

   // Receiving an empty nigori should not reset any encrypted types or trigger
   // an observer notification.
   Mock::VerifyAndClearExpectations(&observer);
   nigori = sync_pb::NigoriSpecifics();
   cryptographer.UpdateEncryptedTypesFromNigori(nigori);
   EXPECT_EQ(encrypted_types, cryptographer.GetEncryptedTypes());
}

TEST(CryptographerTest, EncryptEverythingExplicit) {
  ModelTypeSet real_types = syncable::GetAllRealModelTypes();
  sync_pb::NigoriSpecifics specifics;
  specifics.set_encrypt_everything(true);

  Cryptographer cryptographer;
  StrictMock<MockObserver> observer;
  cryptographer.AddObserver(&observer);

  EXPECT_CALL(observer,
              OnEncryptedTypesChanged(syncable::GetAllRealModelTypes(),
                                      true));

  EXPECT_FALSE(cryptographer.encrypt_everything());
  ModelTypeSet encrypted_types = cryptographer.GetEncryptedTypes();
  for (ModelTypeSet::iterator iter = real_types.begin();
       iter != real_types.end();
       ++iter) {
    if (*iter == syncable::PASSWORDS || *iter == syncable::NIGORI)
      EXPECT_EQ(1U, encrypted_types.count(*iter));
    else
      EXPECT_EQ(0U, encrypted_types.count(*iter));
  }

  cryptographer.UpdateEncryptedTypesFromNigori(specifics);

  EXPECT_TRUE(cryptographer.encrypt_everything());
  encrypted_types = cryptographer.GetEncryptedTypes();
  for (ModelTypeSet::iterator iter = real_types.begin();
       iter != real_types.end();
       ++iter) {
    EXPECT_EQ(1U, encrypted_types.count(*iter));
  }

  // Shouldn't trigger another notification.
  specifics.set_encrypt_everything(true);

  cryptographer.RemoveObserver(&observer);
}

TEST(CryptographerTest, EncryptEverythingImplicit) {
  ModelTypeSet real_types = syncable::GetAllRealModelTypes();
  sync_pb::NigoriSpecifics specifics;
  specifics.set_encrypt_bookmarks(true);  // Non-passwords = encrypt everything

  Cryptographer cryptographer;
  StrictMock<MockObserver> observer;
  cryptographer.AddObserver(&observer);

  EXPECT_CALL(observer,
              OnEncryptedTypesChanged(syncable::GetAllRealModelTypes(),
                                      true));

  EXPECT_FALSE(cryptographer.encrypt_everything());
  ModelTypeSet encrypted_types = cryptographer.GetEncryptedTypes();
  for (ModelTypeSet::iterator iter = real_types.begin();
       iter != real_types.end();
       ++iter) {
    if (*iter == syncable::PASSWORDS || *iter == syncable::NIGORI)
      EXPECT_EQ(1U, encrypted_types.count(*iter));
    else
      EXPECT_EQ(0U, encrypted_types.count(*iter));
  }

  cryptographer.UpdateEncryptedTypesFromNigori(specifics);

  EXPECT_TRUE(cryptographer.encrypt_everything());
  encrypted_types = cryptographer.GetEncryptedTypes();
  for (ModelTypeSet::iterator iter = real_types.begin();
       iter != real_types.end();
       ++iter) {
    EXPECT_EQ(1U, encrypted_types.count(*iter));
  }

  // Shouldn't trigger another notification.
  specifics.set_encrypt_everything(true);

  cryptographer.RemoveObserver(&observer);
}

TEST(CryptographerTest, UnknownSensitiveTypes) {
  ModelTypeSet real_types = syncable::GetAllRealModelTypes();
  sync_pb::NigoriSpecifics specifics;
  // Explicitly setting encrypt everything should override logic for implicit
  // encrypt everything.
  specifics.set_encrypt_everything(false);
  specifics.set_encrypt_bookmarks(true);

  Cryptographer cryptographer;
  StrictMock<MockObserver> observer;
  cryptographer.AddObserver(&observer);

  syncable::ModelTypeSet expected_encrypted_types =
      Cryptographer::SensitiveTypes();
  expected_encrypted_types.insert(syncable::BOOKMARKS);

  EXPECT_CALL(observer,
              OnEncryptedTypesChanged(expected_encrypted_types,
                                      false));

  EXPECT_FALSE(cryptographer.encrypt_everything());
  ModelTypeSet encrypted_types = cryptographer.GetEncryptedTypes();
  for (ModelTypeSet::iterator iter = real_types.begin();
       iter != real_types.end();
       ++iter) {
    if (*iter == syncable::PASSWORDS || *iter == syncable::NIGORI)
      EXPECT_EQ(1U, encrypted_types.count(*iter));
    else
      EXPECT_EQ(0U, encrypted_types.count(*iter));
  }

  cryptographer.UpdateEncryptedTypesFromNigori(specifics);

  EXPECT_FALSE(cryptographer.encrypt_everything());
  encrypted_types = cryptographer.GetEncryptedTypes();
  for (ModelTypeSet::iterator iter = real_types.begin();
       iter != real_types.end();
       ++iter) {
    if (*iter == syncable::PASSWORDS ||
        *iter == syncable::NIGORI ||
        *iter == syncable::BOOKMARKS)
      EXPECT_EQ(1U, encrypted_types.count(*iter));
    else
      EXPECT_EQ(0U, encrypted_types.count(*iter));
  }

  cryptographer.RemoveObserver(&observer);
}

}  // namespace browser_sync
