// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "components/prefs/testing_pref_service.h"
#include "components/update_client/persisted_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

TEST(PersistedDataTest, Simple) {
  std::unique_ptr<TestingPrefServiceSimple> pref(
      new TestingPrefServiceSimple());
  PersistedData::RegisterPrefs(pref->registry());
  std::unique_ptr<PersistedData> metadata(new PersistedData(pref.get()));
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someappid"));
  std::vector<std::string> items;
  items.push_back("someappid");
  metadata->SetDateLastRollCall(items, 3383);
  EXPECT_EQ(3383, metadata->GetDateLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someotherappid"));
  metadata->SetDateLastRollCall(items, 3386);
  EXPECT_EQ(3386, metadata->GetDateLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someotherappid"));
}

TEST(PersistedDataTest, SharedPref) {
  std::unique_ptr<TestingPrefServiceSimple> pref(
      new TestingPrefServiceSimple());
  PersistedData::RegisterPrefs(pref->registry());
  std::unique_ptr<PersistedData> metadata(new PersistedData(pref.get()));
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someappid"));
  std::vector<std::string> items;
  items.push_back("someappid");
  metadata->SetDateLastRollCall(items, 3383);

  // Now, create a new PersistedData reading from the same path, verify
  // that it loads the value.
  metadata.reset(new PersistedData(pref.get()));
  EXPECT_EQ(3383, metadata->GetDateLastRollCall("someappid"));
  EXPECT_EQ(-2, metadata->GetDateLastRollCall("someotherappid"));
}

}  // namespace update_client
