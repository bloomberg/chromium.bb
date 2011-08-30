// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/util/cryptographer.h"
#include "chrome/browser/sync/engine/nigori_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncable {

typedef testing::Test NigoriUtilTest;

TEST_F(NigoriUtilTest, NigoriEncryptionTypes) {
  browser_sync::Cryptographer cryptographer;
  sync_pb::NigoriSpecifics nigori;
  ModelTypeSet encrypted_types;
  encrypted_types.insert(syncable::PASSWORDS);
  FillNigoriEncryptedTypes(encrypted_types, &nigori);
  cryptographer.SetEncryptedTypes(nigori);
  ModelTypeSet test_types = cryptographer.GetEncryptedTypes();
  EXPECT_EQ(encrypted_types, test_types);

  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    encrypted_types.insert(ModelTypeFromInt(i));
  }
  FillNigoriEncryptedTypes(encrypted_types, &nigori);
  cryptographer.SetEncryptedTypes(nigori);
  test_types = cryptographer.GetEncryptedTypes();
  encrypted_types.erase(syncable::NIGORI);     // Should not get set.
  EXPECT_EQ(encrypted_types, test_types);

  encrypted_types.erase(syncable::BOOKMARKS);
  encrypted_types.erase(syncable::SESSIONS);
  FillNigoriEncryptedTypes(encrypted_types, &nigori);
  cryptographer.SetEncryptedTypes(nigori);
  test_types = cryptographer.GetEncryptedTypes();
  EXPECT_EQ(encrypted_types, test_types);
}

TEST(NigoriUtilTest, SpecificsNeedsEncryption) {
  ModelTypeSet encrypted_types;
  encrypted_types.insert(BOOKMARKS);
  encrypted_types.insert(PASSWORDS);

  sync_pb::EntitySpecifics specifics;
  EXPECT_FALSE(SpecificsNeedsEncryption(ModelTypeSet(), specifics));
  EXPECT_FALSE(SpecificsNeedsEncryption(encrypted_types, specifics));

  AddDefaultExtensionValue(PREFERENCES, &specifics);
  EXPECT_FALSE(SpecificsNeedsEncryption(encrypted_types, specifics));

  sync_pb::EntitySpecifics bookmark_specifics;
  AddDefaultExtensionValue(BOOKMARKS, &bookmark_specifics);
  EXPECT_TRUE(SpecificsNeedsEncryption(encrypted_types, bookmark_specifics));

  bookmark_specifics.MutableExtension(sync_pb::bookmark)->set_title("title");
  bookmark_specifics.MutableExtension(sync_pb::bookmark)->set_url("url");
  EXPECT_TRUE(SpecificsNeedsEncryption(encrypted_types, bookmark_specifics));
  EXPECT_FALSE(SpecificsNeedsEncryption(ModelTypeSet(), bookmark_specifics));

  bookmark_specifics.mutable_encrypted();
  EXPECT_FALSE(SpecificsNeedsEncryption(encrypted_types, bookmark_specifics));
  EXPECT_FALSE(SpecificsNeedsEncryption(ModelTypeSet(), bookmark_specifics));

  sync_pb::EntitySpecifics password_specifics;
  AddDefaultExtensionValue(PASSWORDS, &password_specifics);
  EXPECT_FALSE(SpecificsNeedsEncryption(encrypted_types, password_specifics));
}

// ProcessUnsyncedChangesForEncryption and other methods that rely on the syncer
// are tested in apply_updates_command_unittest.cc

}  // namespace syncable
