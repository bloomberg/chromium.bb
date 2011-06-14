// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/cryptographer.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

TEST(CryptographerTest, EmptyCantDecrypt) {
  Cryptographer cryptographer;
  EXPECT_FALSE(cryptographer.is_ready());

  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name("foo");
  encrypted.set_blob("bar");

  EXPECT_FALSE(cryptographer.CanDecrypt(encrypted));
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

}  // namespace browser_sync
