// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/nigori_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncable {

typedef testing::Test NigoriUtilTest;

TEST_F(NigoriUtilTest, NigoriEncryptionTypes) {
  sync_pb::NigoriSpecifics nigori;
  ModelTypeSet encrypted_types;
  FillNigoriEncryptedTypes(encrypted_types, &nigori);
  ModelTypeSet test_types = GetEncryptedDataTypesFromNigori(nigori);
  EXPECT_EQ(encrypted_types, test_types);

  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    encrypted_types.insert(ModelTypeFromInt(i));
  }
  FillNigoriEncryptedTypes(encrypted_types, &nigori);
  test_types = GetEncryptedDataTypesFromNigori(nigori);
  encrypted_types.erase(syncable::NIGORI);     // Should not get set.
  encrypted_types.erase(syncable::PASSWORDS);  // Should not get set.
  EXPECT_EQ(encrypted_types, test_types);

  encrypted_types.erase(syncable::BOOKMARKS);
  encrypted_types.erase(syncable::SESSIONS);
  FillNigoriEncryptedTypes(encrypted_types, &nigori);
  test_types = GetEncryptedDataTypesFromNigori(nigori);
  EXPECT_EQ(encrypted_types, test_types);
}

// ProcessUnsyncedChangesForEncryption and other methods that rely on the syncer
// are tested in apply_updates_command_unittest.cc

}  // namespace syncable
