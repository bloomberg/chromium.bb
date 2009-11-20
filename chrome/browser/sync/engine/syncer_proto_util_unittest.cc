// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_proto_util.h"

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/syncable/blob.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncable::Blob;
using syncable::SyncName;

namespace browser_sync {

TEST(SyncerProtoUtil, TestBlobToProtocolBufferBytesUtilityFunctions) {
  unsigned char test_data1[] = {1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 4, 2, 9};
  unsigned char test_data2[] = {1, 99, 3, 4, 5, 6, 7, 8, 0, 1, 4, 2, 9};
  unsigned char test_data3[] = {99, 2, 3, 4, 5, 6, 7, 8};

  syncable::Blob test_blob1, test_blob2, test_blob3;
  for (size_t i = 0; i < arraysize(test_data1); ++i)
    test_blob1.push_back(test_data1[i]);
  for (size_t i = 0; i < arraysize(test_data2); ++i)
    test_blob2.push_back(test_data2[i]);
  for (size_t i = 0; i < arraysize(test_data3); ++i)
    test_blob3.push_back(test_data3[i]);

  string test_message1(reinterpret_cast<char*>(test_data1),
      arraysize(test_data1));
  string test_message2(reinterpret_cast<char*>(test_data2),
      arraysize(test_data2));
  string test_message3(reinterpret_cast<char*>(test_data3),
      arraysize(test_data3));

  EXPECT_TRUE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message1,
                                                    test_blob1));
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message1,
                                                     test_blob2));
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message1,
                                                     test_blob3));
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message2,
                                                     test_blob1));
  EXPECT_TRUE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message2,
                                                    test_blob2));
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message2,
                                                     test_blob3));
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message3,
                                                     test_blob1));
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message3,
                                                     test_blob2));
  EXPECT_TRUE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message3,
                                                    test_blob3));

  Blob blob1_copy;
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message1,
                                                     blob1_copy));
  SyncerProtoUtil::CopyProtoBytesIntoBlob(test_message1, &blob1_copy);
  EXPECT_TRUE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message1,
                                                    blob1_copy));

  std::string message2_copy;
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(message2_copy,
                                                     test_blob2));
  SyncerProtoUtil::CopyBlobIntoProtoBytes(test_blob2, &message2_copy);
  EXPECT_TRUE(SyncerProtoUtil::ProtoBytesEqualsBlob(message2_copy,
                                                    test_blob2));
}

// Tests NameFromSyncEntity and NameFromCommitEntryResponse when only the name
// field is provided.
TEST(SyncerProtoUtil, NameExtractionOneName) {
  SyncEntity one_name_entity;
  CommitResponse_EntryResponse one_name_response;

  const std::string one_name_string("Eggheadednesses");
  one_name_entity.set_name(one_name_string);
  one_name_response.set_name(one_name_string);

  const std::string name_a =
      SyncerProtoUtil::NameFromSyncEntity(one_name_entity);
  EXPECT_EQ(one_name_string, name_a);

  const std::string name_b =
      SyncerProtoUtil::NameFromCommitEntryResponse(one_name_response);
  EXPECT_EQ(one_name_string, name_b);
  EXPECT_TRUE(name_a == name_b);
}

TEST(SyncerProtoUtil, NameExtractionOneUniqueName) {
  SyncEntity one_name_entity;
  CommitResponse_EntryResponse one_name_response;

  const std::string one_name_string("Eggheadednesses");

  one_name_entity.set_non_unique_name(one_name_string);
  one_name_response.set_non_unique_name(one_name_string);

  const std::string name_a =
      SyncerProtoUtil::NameFromSyncEntity(one_name_entity);
  EXPECT_EQ(one_name_string, name_a);

  const std::string name_b =
      SyncerProtoUtil::NameFromCommitEntryResponse(one_name_response);
  EXPECT_EQ(one_name_string, name_b);
  EXPECT_TRUE(name_a == name_b);
}

// Tests NameFromSyncEntity and NameFromCommitEntryResponse when both the name
// field and the non_unique_name fields are provided.
// Should prioritize non_unique_name.
TEST(SyncerProtoUtil, NameExtractionTwoNames) {
  SyncEntity two_name_entity;
  CommitResponse_EntryResponse two_name_response;

  const std::string neuro("Neuroanatomists");
  const std::string oxyphen("Oxyphenbutazone");

  two_name_entity.set_name(oxyphen);
  two_name_entity.set_non_unique_name(neuro);

  two_name_response.set_name(oxyphen);
  two_name_response.set_non_unique_name(neuro);

  const std::string name_a =
      SyncerProtoUtil::NameFromSyncEntity(two_name_entity);
  EXPECT_EQ(neuro, name_a);

  const std::string name_b =
      SyncerProtoUtil::NameFromCommitEntryResponse(two_name_response);
  EXPECT_EQ(neuro, name_b);

  EXPECT_TRUE(name_a == name_b);
}

}  // namespace browser_sync
