// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/dictionary_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"

namespace {

// Class that enables or disables USS based on test parameter. Must be the first
// base class of the test fixture.
class UssSwitchToggler : public testing::WithParamInterface<bool> {
 public:
  UssSwitchToggler() {
    if (GetParam()) {
      override_features_.InitAndEnableFeature(
          switches::kSyncPseudoUSSDictionary);
    } else {
      override_features_.InitAndDisableFeature(
          switches::kSyncPseudoUSSDictionary);
    }
  }

 private:
  base::test::ScopedFeatureList override_features_;
};

class SingleClientDictionarySyncTest : public UssSwitchToggler,
                                       public SyncTest {
 public:
  SingleClientDictionarySyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientDictionarySyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientDictionarySyncTest);
};

IN_PROC_BROWSER_TEST_P(SingleClientDictionarySyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  std::string word = "foo";
  ASSERT_TRUE(dictionary_helper::AddWord(0, word));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  ASSERT_TRUE(dictionary_helper::RemoveWord(0, word));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());
}

INSTANTIATE_TEST_CASE_P(USS,
                        SingleClientDictionarySyncTest,
                        ::testing::Values(false, true));

}  // namespace
