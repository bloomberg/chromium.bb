// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_proto_util.h"

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/syncable/blob.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/test/engine/mock_connection_manager.h"
#include "chrome/browser/sync/test/engine/test_directory_setter_upper.h"

#include "testing/gtest/include/gtest/gtest.h"

using syncable::Blob;
using syncable::ScopedDirLookup;

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

  std::string test_message1(reinterpret_cast<char*>(test_data1),
      arraysize(test_data1));
  std::string test_message2(reinterpret_cast<char*>(test_data2),
      arraysize(test_data2));
  std::string test_message3(reinterpret_cast<char*>(test_data3),
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
}

class SyncerProtoUtilTest : public testing::Test {
 public:
  virtual void SetUp() {
    setter_upper_.SetUp();
  }

  virtual void TearDown() {
    setter_upper_.TearDown();
  }

 protected:
  MessageLoop message_loop_;
  browser_sync::TestDirectorySetterUpper setter_upper_;
};

TEST_F(SyncerProtoUtilTest, VerifyResponseBirthday) {
  ScopedDirLookup lookup(setter_upper_.manager(), setter_upper_.name());
  ASSERT_TRUE(lookup.good());

  // Both sides empty
  EXPECT_TRUE(lookup->store_birthday().empty());
  ClientToServerResponse response;
  EXPECT_FALSE(SyncerProtoUtil::VerifyResponseBirthday(lookup, &response));

  // Remote set, local empty
  response.set_store_birthday("flan");
  EXPECT_TRUE(SyncerProtoUtil::VerifyResponseBirthday(lookup, &response));
  EXPECT_EQ(lookup->store_birthday(), "flan");

  // Remote empty, local set.
  response.clear_store_birthday();
  EXPECT_TRUE(SyncerProtoUtil::VerifyResponseBirthday(lookup, &response));
  EXPECT_EQ(lookup->store_birthday(), "flan");

  // Doesn't match
  response.set_store_birthday("meat");
  EXPECT_FALSE(SyncerProtoUtil::VerifyResponseBirthday(lookup, &response));

  response.set_error_code(ClientToServerResponse::CLEAR_PENDING);
  EXPECT_FALSE(SyncerProtoUtil::VerifyResponseBirthday(lookup, &response));
}

TEST_F(SyncerProtoUtilTest, AddRequestBirthday) {
  ScopedDirLookup lookup(setter_upper_.manager(), setter_upper_.name());
  ASSERT_TRUE(lookup.good());

  EXPECT_TRUE(lookup->store_birthday().empty());
  ClientToServerMessage msg;
  SyncerProtoUtil::AddRequestBirthday(lookup, &msg);
  EXPECT_FALSE(msg.has_store_birthday());

  lookup->set_store_birthday("meat");
  SyncerProtoUtil::AddRequestBirthday(lookup, &msg);
  EXPECT_EQ(msg.store_birthday(), "meat");
}

class DummyConnectionManager : public browser_sync::ServerConnectionManager {
 public:
  DummyConnectionManager()
      : ServerConnectionManager("unused", 0, false, "version"),
        send_error_(false),
        access_denied_(false) {}

  virtual ~DummyConnectionManager() {}
  virtual bool PostBufferWithCachedAuth(
      PostBufferParams* params,
      ScopedServerStatusWatcher* watcher) OVERRIDE {
    if (send_error_) {
      return false;
    }

    ClientToServerResponse response;
    if (access_denied_) {
      response.set_error_code(ClientToServerResponse::ACCESS_DENIED);
    }
    response.SerializeToString(&params->buffer_out);

    return true;
  }

  void set_send_error(bool send) {
    send_error_ = send;
  }

  void set_access_denied(bool denied) {
    access_denied_ = denied;
  }

 private:
  bool send_error_;
  bool access_denied_;
};

TEST_F(SyncerProtoUtilTest, PostAndProcessHeaders) {
  DummyConnectionManager dcm;
  ClientToServerMessage msg;
  msg.set_share("required");
  msg.set_message_contents(ClientToServerMessage::GET_UPDATES);
  ClientToServerResponse response;

  dcm.set_send_error(true);
  EXPECT_FALSE(SyncerProtoUtil::PostAndProcessHeaders(&dcm, NULL,
      msg, &response));

  dcm.set_send_error(false);
  EXPECT_TRUE(SyncerProtoUtil::PostAndProcessHeaders(&dcm, NULL,
      msg, &response));

  dcm.set_access_denied(true);
  EXPECT_FALSE(SyncerProtoUtil::PostAndProcessHeaders(&dcm, NULL,
      msg, &response));
}

}  // namespace browser_sync
