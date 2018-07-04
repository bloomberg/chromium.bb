// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/url_keyed_anonymized_data_collection_consent_helper.h"

#include <vector>

#include "components/sync/driver/fake_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unified_consent {
namespace {

class UrlKeyedDataCollectionConsentHelperTest
    : public testing::Test,
      public UrlKeyedAnonymizedDataCollectionConsentHelper::Observer {
 public:
  // testing::Test:
  void SetUp() override {
    UnifiedConsentService::RegisterPrefs(pref_service_.registry());
  }

  void OnUrlKeyedDataCollectionConsentStateChanged(bool enabled) override {
    state_changed_notifications.push_back(enabled);
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::vector<bool> state_changed_notifications;
  syncer::FakeSyncService sync_service_;
};

TEST_F(UrlKeyedDataCollectionConsentHelperTest, UnifiedConsentEnabled) {
  std::unique_ptr<UrlKeyedAnonymizedDataCollectionConsentHelper> helper =
      UrlKeyedAnonymizedDataCollectionConsentHelper::NewInstance(
          true, &pref_service_, &sync_service_);
  helper->AddObserver(this);
  EXPECT_FALSE(helper->IsEnabled());
  EXPECT_TRUE(state_changed_notifications.empty());

  pref_service_.SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                           true);
  EXPECT_TRUE(helper->IsEnabled());
  EXPECT_EQ(1U, state_changed_notifications.size());
  EXPECT_TRUE(state_changed_notifications[0]);

  state_changed_notifications.clear();
  pref_service_.SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                           false);
  EXPECT_FALSE(helper->IsEnabled());
  EXPECT_EQ(1U, state_changed_notifications.size());
  EXPECT_FALSE(state_changed_notifications[0]);
  helper->RemoveObserver(this);
}

TEST_F(UrlKeyedDataCollectionConsentHelperTest, UnifiedConsentDisabled) {
  std::unique_ptr<UrlKeyedAnonymizedDataCollectionConsentHelper> helper =
      UrlKeyedAnonymizedDataCollectionConsentHelper::NewInstance(
          false, &pref_service_, &sync_service_);
  EXPECT_FALSE(helper->IsEnabled());
}

}  // namespace
}  // namespace unified_consent
