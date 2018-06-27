// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_profile_sync_util.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using base::ASCIIToUTF16;
using base::ScopedTempDir;
using base::UTF16ToUTF8;
using base::UTF8ToUTF16;
using sync_pb::AutofillProfileSpecifics;
using syncer::DataBatch;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::EntityDataPtr;
using syncer::KeyAndData;
using syncer::MockModelTypeChangeProcessor;
using syncer::ModelType;
using testing::_;
using testing::DoAll;
using testing::ElementsAre;
using testing::Eq;
using testing::Property;
using testing::Return;

namespace {

// Some guids for testing.
const char kGuidA[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44A";
const char kGuidB[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
const char kHttpsOrigin[] = "https://www.example.com/";
const int kValidityStateBitfield = 1984;
const char kLocaleString[] = "en-US";
const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);

class FakeAutofillBackend : public AutofillWebDataBackend {
 public:
  FakeAutofillBackend() {}
  ~FakeAutofillBackend() override {}
  WebDatabase* GetDatabase() override { return db_; }
  void AddObserver(
      autofill::AutofillWebDataServiceObserverOnDBSequence* observer) override {
  }
  void RemoveObserver(
      autofill::AutofillWebDataServiceObserverOnDBSequence* observer) override {
  }
  void RemoveExpiredFormElements() override {}
  void NotifyOfMultipleAutofillChanges() override {}
  void NotifyThatSyncHasStarted(ModelType model_type) override {}
  void SetWebDatabase(WebDatabase* db) { db_ = db; }

 private:
  WebDatabase* db_;
};

AutofillProfile CreateAutofillProfile(
    const AutofillProfileSpecifics& specifics) {
  // As more copying does not hurt in tests, we prefer to use AutofillProfile
  // instead of std::unique_ptr<AutofillProfile> because of code brevity.
  return *CreateAutofillProfileFromSpecifics(specifics);
}

AutofillProfileSpecifics CreateAutofillProfileSpecifics(
    const AutofillProfile& entry) {
  // Reuse production code. We do not need EntityData, just take out the
  // specifics.
  std::unique_ptr<EntityData> entity_data =
      CreateEntityDataFromAutofillProfile(entry);
  return entity_data->specifics.autofill_profile();
}

MATCHER_P(HasSpecifics, expected, "") {
  const AutofillProfileSpecifics& s1 = arg->specifics.autofill_profile();
  const AutofillProfileSpecifics& s2 = expected;

  AutofillProfile p1 = CreateAutofillProfile(s1);
  AutofillProfile p2 = CreateAutofillProfile(s2);
  if (!p1.EqualsIncludingUsageStatsForTesting(p2)) {
    *result_listener << "entry\n[" << p1 << "]\n"
                     << "did not match expected\n[" << p2 << "]";
    return false;
  }
  return true;
}

void ExtractAutofillProfilesFromDataBatch(
    std::unique_ptr<DataBatch> batch,
    std::vector<AutofillProfile>* output) {
  while (batch->HasNext()) {
    const KeyAndData& data_pair = batch->Next();
    output->push_back(
        CreateAutofillProfile(data_pair.second->specifics.autofill_profile()));
  }
}

}  // namespace

class AutofillProfileSyncBridgeTest : public testing::Test {
 public:
  AutofillProfileSyncBridgeTest() {}
  ~AutofillProfileSyncBridgeTest() override {}

  void SetUp() override {
    // Fix a time for implicitly constructed use_dates in AutofillProfile.
    test_clock_.SetNow(kJune2017);
    CountryNames::SetLocaleString(kLocaleString);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    backend_.SetWebDatabase(&db_);
    ResetProcessor();
    ResetBridge();
  }

  void ResetProcessor() {
    real_processor_ =
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::AUTOFILL_PROFILE, /*dump_stack=*/base::DoNothing(),
            /*commit_only=*/false);
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge() {
    bridge_.reset(new AutofillProfileSyncBridge(
        mock_processor_.CreateForwardingProcessor(), kLocaleString, &backend_));
  }

  void StartSyncing(
      const std::vector<AutofillProfileSpecifics>& remote_data = {}) {
    base::RunLoop loop;
    syncer::DataTypeActivationRequest request;
    request.error_handler = base::DoNothing();
    real_processor_->OnSyncStarting(
        request,
        base::BindLambdaForTesting(
            [&loop](std::unique_ptr<syncer::DataTypeActivationResponse>) {
              loop.Quit();
            }));
    loop.Run();

    // Initialize the processor with initial_sync_done.
    sync_pb::ModelTypeState state;
    state.set_initial_sync_done(true);
    syncer::UpdateResponseDataList initial_updates;
    for (const AutofillProfileSpecifics& specifics : remote_data) {
      initial_updates.push_back(SpecificsToUpdateResponse(specifics));
    }
    real_processor_->OnUpdateReceived(state, initial_updates);
  }

  void AddAutofillProfilesToTable(
      const std::vector<AutofillProfile>& profile_list) {
    for (const auto& profile : profile_list) {
      table_.AddAutofillProfile(profile);
    }
  }

  std::vector<AutofillProfile> GetAllData() {
    std::vector<AutofillProfile> data;
    // Perform an async call synchronously for testing.
    base::RunLoop loop;
    bridge()->GetAllDataForDebugging(base::BindLambdaForTesting(
        [&loop, &data](std::unique_ptr<DataBatch> batch) {
          ExtractAutofillProfilesFromDataBatch(std::move(batch), &data);
          loop.Quit();
        }));
    loop.Run();
    return data;
  }

  EntityDataPtr SpecificsToEntity(const AutofillProfileSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_autofill_profile() = specifics;
    data.client_tag_hash = syncer::GenerateSyncableHash(
        syncer::AUTOFILL_PROFILE, bridge()->GetClientTag(data));
    return data.PassToPtr();
  }

  syncer::UpdateResponseData SpecificsToUpdateResponse(
      const AutofillProfileSpecifics& specifics) {
    syncer::UpdateResponseData data;
    data.entity = SpecificsToEntity(specifics);
    return data;
  }

  AutofillProfileSyncBridge* bridge() { return bridge_.get(); }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  AutofillTable* table() { return &table_; }

  FakeAutofillBackend* backend() { return &backend_; }

 private:
  autofill::TestAutofillClock test_clock_;
  ScopedTempDir temp_dir_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  FakeAutofillBackend backend_;
  AutofillTable table_;
  WebDatabase db_;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor> real_processor_;
  std::unique_ptr<AutofillProfileSyncBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileSyncBridgeTest);
};

TEST_F(AutofillProfileSyncBridgeTest, AutofillProfileChanged_Added) {
  StartSyncing({});

  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuidA, &local);

  EXPECT_CALL(
      mock_processor(),
      Put(kGuidA, HasSpecifics(CreateAutofillProfileSpecifics(local)), _));

  bridge()->AutofillProfileChanged(change);
}

// Language code in autofill profiles should be synced to the server.
TEST_F(AutofillProfileSyncBridgeTest,
       AutofillProfileChanged_Added_LanguageCodePropagates) {
  StartSyncing({});

  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.set_language_code("en");
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuidA, &local);

  EXPECT_CALL(
      mock_processor(),
      Put(kGuidA, HasSpecifics(CreateAutofillProfileSpecifics(local)), _));

  bridge()->AutofillProfileChanged(change);
}

// Validity state bitfield in autofill profiles should be synced to the server.
TEST_F(AutofillProfileSyncBridgeTest,
       AutofillProfileChanged_Added_LocalValidityBitfieldPropagates) {
  StartSyncing({});

  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.SetValidityFromBitfieldValue(kValidityStateBitfield);
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuidA, &local);

  EXPECT_CALL(
      mock_processor(),
      Put(kGuidA, HasSpecifics(CreateAutofillProfileSpecifics(local)), _));

  bridge()->AutofillProfileChanged(change);
}

// Local updates should be properly propagated to the server.
TEST_F(AutofillProfileSyncBridgeTest, AutofillProfileChanged_Updated) {
  StartSyncing({});

  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  AutofillProfileChange change(AutofillProfileChange::UPDATE, kGuidA, &local);

  EXPECT_CALL(
      mock_processor(),
      Put(kGuidA, HasSpecifics(CreateAutofillProfileSpecifics(local)), _));

  bridge()->AutofillProfileChanged(change);
}

// Server profile updates should be ignored.
TEST_F(AutofillProfileSyncBridgeTest,
       AutofillProfileChanged_Updated_IgnoreServerProfiles) {
  StartSyncing({});

  AutofillProfile server_profile(AutofillProfile::SERVER_PROFILE, "server-id");
  AutofillProfileChange change(AutofillProfileChange::UPDATE,
                               server_profile.guid(), &server_profile);

  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  // Should not crash.
  bridge()->AutofillProfileChanged(change);
}

TEST_F(AutofillProfileSyncBridgeTest, AutofillProfileChanged_Deleted) {
  StartSyncing({});

  AutofillProfileChange change(AutofillProfileChange::REMOVE, kGuidB, nullptr);
  EXPECT_CALL(mock_processor(), Delete(kGuidB, _));

  bridge()->AutofillProfileChanged(change);
}

TEST_F(AutofillProfileSyncBridgeTest, GetAllDataForDebugging) {
  AutofillProfile local1 = AutofillProfile(kGuidA, kHttpsOrigin);
  local1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  local1.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  AutofillProfile local2 = AutofillProfile(kGuidB, kHttpsOrigin);
  local2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  local2.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("2 2nd st"));
  AddAutofillProfilesToTable({local1, local2});

  EXPECT_THAT(GetAllData(), ElementsAre(local1, local2));
}

TEST_F(AutofillProfileSyncBridgeTest, GetData) {
  AutofillProfile local1 = AutofillProfile(kGuidA, kHttpsOrigin);
  local1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  local1.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  AutofillProfile local2 = AutofillProfile(kGuidB, kHttpsOrigin);
  local2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  local2.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("2 2nd st"));
  AddAutofillProfilesToTable({local1, local2});

  std::vector<AutofillProfile> data;
  base::RunLoop loop;
  bridge()->GetData({kGuidA},
                    base::BindLambdaForTesting(
                        [&loop, &data](std::unique_ptr<DataBatch> batch) {
                          ExtractAutofillProfilesFromDataBatch(std::move(batch),
                                                               &data);
                          loop.Quit();
                        }));
  loop.Run();

  EXPECT_THAT(data, ElementsAre(local1));
}

// Verifies that setting the street address field also sets the (deprecated)
// address line 1 and line 2 fields.
TEST_F(AutofillProfileSyncBridgeTest, StreetAddress_SplitAutomatically) {
  AutofillProfile local;
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("123 Example St.\n"
                                                             "Apt. 42"));
  EXPECT_EQ(ASCIIToUTF16("123 Example St."),
            local.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(ASCIIToUTF16("Apt. 42"), local.GetRawInfo(ADDRESS_HOME_LINE2));

  // The same does _not_ work for profile specifics.
  AutofillProfileSpecifics remote;
  remote.set_address_home_street_address(
      "123 Example St.\n"
      "Apt. 42");
  EXPECT_FALSE(remote.has_address_home_line1());
  EXPECT_FALSE(remote.has_address_home_line2());
}

// Verifies that setting the (deprecated) address line 1 and line 2 fields also
// sets the street address.
TEST_F(AutofillProfileSyncBridgeTest, StreetAddress_JointAutomatically) {
  AutofillProfile local;
  local.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Example St."));
  local.SetRawInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("Apt. 42"));
  EXPECT_EQ(ASCIIToUTF16("123 Example St.\n"
                         "Apt. 42"),
            local.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));

  // The same does _not_ work for profile specifics.
  AutofillProfileSpecifics remote;
  remote.set_address_home_line1("123 Example St.");
  remote.set_address_home_line2("Apt. 42");
  EXPECT_FALSE(remote.has_address_home_street_address());
}

}  // namespace autofill
