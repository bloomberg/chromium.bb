// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_non_recording_data_store.h"

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_inspector.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace resource_coordinator {

namespace {
// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
url::Origin TestOrigin() {
  return url::Origin::Create(GURL("http://www.foo.com"));
}

class LocalSiteCharacteristicsNonRecordingDataStoreTest : public testing::Test {
 public:
  void SetUp() override {
    recording_data_store_ =
        std::make_unique<LocalSiteCharacteristicsDataStore>(&parent_profile_);
    non_recording_data_store_ =
        std::make_unique<LocalSiteCharacteristicsNonRecordingDataStore>(
            &profile_, recording_data_store_.get(),
            recording_data_store_.get());

    WaitForAsyncOperationsToComplete();
  }

  void WaitForAsyncOperationsToComplete() { task_environment_.RunUntilIdle(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile parent_profile_;
  TestingProfile profile_;
  std::unique_ptr<LocalSiteCharacteristicsDataStore> recording_data_store_;
  std::unique_ptr<LocalSiteCharacteristicsNonRecordingDataStore>
      non_recording_data_store_;
};

}  // namespace

TEST_F(LocalSiteCharacteristicsNonRecordingDataStoreTest, EndToEnd) {
  // Ensures that the observation made via a writer created by the non
  // recording data store aren't recorded.
  auto reader = non_recording_data_store_->GetReaderForOrigin(TestOrigin());
  EXPECT_TRUE(reader);
  auto fake_writer = non_recording_data_store_->GetWriterForOrigin(
      TestOrigin(), performance_manager::TabVisibility::kBackground);
  EXPECT_TRUE(fake_writer);
  auto real_writer = recording_data_store_->GetWriterForOrigin(
      TestOrigin(), performance_manager::TabVisibility::kBackground);
  EXPECT_TRUE(real_writer);

  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());
  fake_writer->NotifySiteLoaded();
  fake_writer->NotifyUpdatesTitleInBackground();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());

  real_writer->NotifySiteLoaded();
  real_writer->NotifyUpdatesTitleInBackground();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());

  // These unload events shouldn't be registered, make sure that they aren't by
  // unloading the site more time than it has been loaded.
  fake_writer->NotifySiteUnloaded();
  fake_writer->NotifySiteUnloaded();

  real_writer->NotifySiteUnloaded();

  real_writer.reset();
  reader.reset();

  WaitForAsyncOperationsToComplete();

  recording_data_store_.reset();

  // Wait for the database of the recording store to be flushed to disk before
  // terminating this test, otherwise the profile might get deleted while the
  // database is still being flushed to disk and this could prevent from
  // deleting its temporary directory.
  WaitForAsyncOperationsToComplete();
}

TEST_F(LocalSiteCharacteristicsNonRecordingDataStoreTest, InspectorWorks) {
  // Make sure the inspector interface was registered at construction.
  LocalSiteCharacteristicsDataStoreInspector* inspector =
      LocalSiteCharacteristicsDataStoreInspector::GetForProfile(&profile_);
  EXPECT_NE(nullptr, inspector);
  EXPECT_EQ(non_recording_data_store_.get(), inspector);

  EXPECT_STREQ("LocalSiteCharacteristicsNonRecordingDataStore",
               inspector->GetDataStoreName());

  // We expect an empty data store at the outset.
  EXPECT_EQ(0U, inspector->GetAllInMemoryOrigins().size());
  std::unique_ptr<SiteDataProto> data;
  bool is_dirty = false;
  EXPECT_FALSE(inspector->GetDataForOrigin(TestOrigin(), &is_dirty, &data));
  EXPECT_FALSE(is_dirty);
  EXPECT_EQ(nullptr, data.get());

  {
    // Add an entry through the writing data store, see that it's reflected in
    // the inspector interface.
    auto writer = recording_data_store_->GetWriterForOrigin(
        TestOrigin(), performance_manager::TabVisibility::kBackground);

    EXPECT_EQ(1U, inspector->GetAllInMemoryOrigins().size());
    EXPECT_TRUE(inspector->GetDataForOrigin(TestOrigin(), &is_dirty, &data));
    EXPECT_FALSE(is_dirty);
    ASSERT_NE(nullptr, data.get());

    // Touch the underlying data, see that the dirty bit updates.
    writer->NotifySiteLoaded();
    EXPECT_TRUE(inspector->GetDataForOrigin(TestOrigin(), &is_dirty, &data));
  }

  // Make sure the interface is unregistered from the profile on destruction.
  non_recording_data_store_.reset();
  EXPECT_EQ(nullptr, LocalSiteCharacteristicsDataStoreInspector::GetForProfile(
                         &profile_));
}

}  // namespace resource_coordinator
