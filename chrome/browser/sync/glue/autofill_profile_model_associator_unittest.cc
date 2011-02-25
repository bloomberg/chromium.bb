// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/sync/engine/read_node_mock.h"
#include "chrome/browser/sync/engine/syncapi_mock.h"
#include "chrome/browser/sync/glue/autofill_profile_model_associator.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::DoDefault;
using ::testing::ReturnRef;
using ::testing::Pointee;
using ::testing::Ref;
using ::testing::Invoke;
class AutoFillProfile;

using browser_sync::AutofillProfileModelAssociator;

// Note this is not a generic mock. This is a mock used to
// test AutofillProfileModelAssociator class itself by mocking
// other functions that are called by the functions we need to test.
class MockAutofillProfileModelAssociator
    : public AutofillProfileModelAssociator {
 public:
  MockAutofillProfileModelAssociator() {
  }
  virtual ~MockAutofillProfileModelAssociator() {}
  bool TraverseAndAssociateChromeAutoFillProfilesWrapper(
      sync_api::WriteTransaction* write_trans,
      const sync_api::ReadNode& autofill_root,
      const std::vector<AutoFillProfile*>& all_profiles_from_db,
      std::set<std::string>* current_profiles,
      std::vector<AutoFillProfile*>* updated_profiles,
      std::vector<AutoFillProfile*>* new_profiles,
      std::vector<std::string>* profiles_to_delete) {
      return TraverseAndAssociateChromeAutoFillProfiles(write_trans,
          autofill_root,
          all_profiles_from_db,
          current_profiles,
          updated_profiles,
          new_profiles,
          profiles_to_delete);
  }
  MOCK_METHOD3(AddNativeProfileIfNeeded,
               void(const sync_pb::AutofillProfileSpecifics&,
                    DataBundle*,
                    const sync_api::ReadNode&));
  MOCK_METHOD2(OverwriteProfileWithServerData,
               bool(AutoFillProfile*,
                    const sync_pb::AutofillProfileSpecifics&));
  MOCK_METHOD6(MakeNewAutofillProfileSyncNodeIfNeeded,
               bool(sync_api::WriteTransaction*,
                    const sync_api::BaseNode&,
                    const AutoFillProfile&,
                    std::vector<AutoFillProfile*>*,
                    std::set<std::string>*,
                    std::vector<std::string>*));
  MOCK_METHOD2(Associate, void(const std::string*, int64));

  bool TraverseAndAssociateAllSyncNodesWrapper(
      sync_api::WriteTransaction *trans,
      const sync_api::ReadNode &autofill_root,
      browser_sync::AutofillProfileModelAssociator::DataBundle *bundle) {
      return TraverseAndAssociateAllSyncNodes(trans, autofill_root, bundle);
  }

  void AddNativeProfileIfNeededWrapper(
      const sync_pb::AutofillProfileSpecifics& profile,
      DataBundle* bundle,
      const sync_api::ReadNode& node) {
    AutofillProfileModelAssociator::AddNativeProfileIfNeeded(
        profile,
        bundle,
        node);
  }
};

class AutofillProfileModelAssociatorTest : public testing::Test {
 public:
  AutofillProfileModelAssociatorTest()
    : db_thread_(BrowserThread::DB, &message_loop_) {}

 protected:
  MessageLoop message_loop_;
  BrowserThread db_thread_;
  MockAutofillProfileModelAssociator associator_;
};

TEST_F(AutofillProfileModelAssociatorTest,
    TestAssociateProfileInWebDBWithSyncDB) {
  ScopedVector<AutoFillProfile> profiles_from_web_db;
  std::string guid = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";

  sync_pb::EntitySpecifics specifics;
  MockDirectory mock_directory;
  sync_pb::AutofillProfileSpecifics *profile_specifics =
    specifics.MutableExtension(sync_pb::autofill_profile);

  profile_specifics->set_guid(guid);

  std::set<std::string> current_profiles;

  // This will be released inside the function
  // TraverseAndAssociateChromeAutofillProfiles
  AutoFillProfile *profile = new AutoFillProfile(guid);

  // Set up the entry kernel with what we want.
  EntryKernel kernel;
  kernel.put(syncable::SPECIFICS, specifics);
  kernel.put(syncable::META_HANDLE, 1);

  MockWriteTransaction write_trans(&mock_directory);
  EXPECT_CALL(mock_directory, GetEntryByClientTag(_))
             .WillOnce(Return(&kernel));

  sync_api::ReadNode read_node(&write_trans);

  EXPECT_CALL(associator_, Associate(Pointee(guid), 1));

  profiles_from_web_db.push_back(profile);

  associator_.TraverseAndAssociateChromeAutoFillProfilesWrapper(&write_trans,
      read_node,
      profiles_from_web_db.get(),
      &current_profiles,
      NULL,
      NULL,
      NULL);

  EXPECT_EQ((unsigned int)1, current_profiles.size());
}

TEST_F(AutofillProfileModelAssociatorTest, TestAssociatingMissingWebDBProfile) {
  ScopedVector<AutoFillProfile> profiles_from_web_db;
  MockDirectory mock_directory;

  MockWriteTransaction write_trans(&mock_directory);
  EXPECT_CALL(mock_directory,
              GetEntryByClientTag(_))
             .WillOnce(Return(reinterpret_cast<EntryKernel*>(NULL)));

  sync_api::ReadNode autofill_root(&write_trans);

  std::string guid = "EDC609ED-7EEE-4F27-B00C-423242A9C44A";
  std::set<std::string> current_profiles;
  AutoFillProfile *profile = new AutoFillProfile(guid);

  EXPECT_CALL(associator_,
              MakeNewAutofillProfileSyncNodeIfNeeded(&write_trans,
                                             Ref(autofill_root),
                                             Ref(*profile),
                                             _,
                                             &current_profiles,
                                             NULL))
              .WillOnce(Return(true));

  profiles_from_web_db.push_back(profile);

  associator_.TraverseAndAssociateChromeAutoFillProfilesWrapper(&write_trans,
      autofill_root,
      profiles_from_web_db.get(),
      &current_profiles,
      NULL,
      NULL,
      NULL);
}

TEST_F(AutofillProfileModelAssociatorTest,
    TestAssociateProfileInSyncDBWithWebDB) {
  ReadNodeMock autofill_root;

  // The constrcutor itself will initialize the id to root.
  syncable::Id root_id;

  sync_pb::EntitySpecifics specifics;
  MockDirectory mock_directory;
  sync_pb::AutofillProfileSpecifics *profile_specifics =
      specifics.MutableExtension(sync_pb::autofill_profile);

  profile_specifics->set_guid("abc");

  // Set up the entry kernel with what we want.
  EntryKernel kernel;
  kernel.put(syncable::SPECIFICS, specifics);
  kernel.put(syncable::META_HANDLE, 1);
  kernel.put(syncable::ID, root_id);

  MockWriteTransaction write_trans(&mock_directory);

  browser_sync::AutofillProfileModelAssociator::DataBundle bundle;

  EXPECT_CALL(autofill_root, GetFirstChildId())
              .WillOnce(Return(1));

  EXPECT_CALL(mock_directory,
              GetEntryByHandle(_))
             .WillOnce(Return(&kernel));

  EXPECT_CALL(associator_,
    AddNativeProfileIfNeeded(_,
        &bundle,
        _));

  associator_.TraverseAndAssociateAllSyncNodesWrapper(&write_trans,
      autofill_root,
      &bundle);
}

TEST_F(AutofillProfileModelAssociatorTest, TestDontNeedToAddNativeProfile) {
  ::testing::StrictMock<MockAutofillProfileModelAssociator> associator;
  sync_pb::AutofillProfileSpecifics profile_specifics;
  ReadNodeMock read_node;
  std::string guid = "EDC609ED-7EEE-4F27-B00C-423242A9C44C";
  std::set<std::string> current_profiles;
  browser_sync::AutofillProfileModelAssociator::DataBundle bundle;

  profile_specifics.set_guid(guid);

  bundle.current_profiles.insert(guid);

  // We have no expectations to set. We have used a strict mock.
  // If the profile is already present no other function
  // should be called.
  associator.AddNativeProfileIfNeededWrapper(profile_specifics, &bundle,
      read_node);
}

TEST_F(AutofillProfileModelAssociatorTest, TestNeedToAddNativeProfile) {
  sync_pb::AutofillProfileSpecifics profile_specifics;
  ReadNodeMock read_node;
  std::string guid = "EDC609ED-7EEE-4F27-B00C-423242A9C44D";
  std::set<std::string> current_profiles;
  browser_sync::AutofillProfileModelAssociator::DataBundle bundle;
  std::string first_name = "lingesh";

  profile_specifics.set_guid(guid);
  profile_specifics.set_name_first(first_name);

  EXPECT_CALL(read_node, GetId())
              .WillOnce(Return(1));

  EXPECT_CALL(associator_,
    Associate(Pointee(guid), 1));

  associator_.AddNativeProfileIfNeededWrapper(
      profile_specifics,
      &bundle,
      read_node);

  EXPECT_EQ(bundle.new_profiles.size(), (unsigned int)1);
  EXPECT_EQ(
      bundle.new_profiles.front()->GetFieldText(AutoFillType(NAME_FIRST)),
      UTF8ToUTF16(first_name));
}

