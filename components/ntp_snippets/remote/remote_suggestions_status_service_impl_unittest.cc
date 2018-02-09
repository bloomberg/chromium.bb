// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_status_service_impl.h"

#include <memory>

#include "build/build_config.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/variations_params_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

namespace {
const char kTestPrefName[] = "search_suggestions.test_name";

void OnStatusChange(RemoteSuggestionsStatus old_status,
                    RemoteSuggestionsStatus new_status) {}

}  // namespace

class RemoteSuggestionsStatusServiceImplTest : public ::testing::Test {
 public:
  RemoteSuggestionsStatusServiceImplTest() {
    RemoteSuggestionsStatusServiceImpl::RegisterProfilePrefs(
        utils_.pref_service()->registry());

    // Registering additional test preference for testing serch suggestion based
    // feature disabling.
    utils_.pref_service()->registry()->RegisterBooleanPref(kTestPrefName, true);
  }

  // |list_hiding_enabled| indicates whether kArticleSuggestionsExpandableHeader
  // is enabled.
  std::unique_ptr<RemoteSuggestionsStatusServiceImpl> MakeService(
      bool list_hiding_enabled) {
    auto service = std::make_unique<RemoteSuggestionsStatusServiceImpl>(
        false, utils_.pref_service(),
        list_hiding_enabled ? std::string() : kTestPrefName);
    service->Init(base::BindRepeating(&OnStatusChange));
    return service;
  }

 protected:
  test::RemoteSuggestionsTestUtils utils_;
  variations::testing::VariationParamsManager params_manager_;
};

TEST_F(RemoteSuggestionsStatusServiceImplTest, NoSigninNeeded) {
  auto service = MakeService(/*list_hiding_enabled=*/false);

  // By default, no signin is required.
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT,
            service->GetStatusFromDeps());

  // Signin should cause a state change.
  service->OnSignInStateChanged(/*has_signed_in=*/true);
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN,
            service->GetStatusFromDeps());
}

TEST_F(RemoteSuggestionsStatusServiceImplTest, DisabledViaPref) {
  auto service = MakeService(/*list_hiding_enabled=*/false);

  // The default test setup is signed out. The service is enabled.
  ASSERT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT,
            service->GetStatusFromDeps());

  // Once the enabled pref is set to false, we should be disabled.
  utils_.pref_service()->SetBoolean(prefs::kEnableSnippets, false);
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED,
            service->GetStatusFromDeps());

  // The state should not change, even though a signin has occurred.
  service->OnSignInStateChanged(/*has_signed_in=*/true);
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED,
            service->GetStatusFromDeps());
}

TEST_F(RemoteSuggestionsStatusServiceImplTest, DisabledViaAdditionalPref) {
  auto service = MakeService(/*list_hiding_enabled=*/false);

  // The default test setup is signed out. The service is enabled.
  ASSERT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT,
            service->GetStatusFromDeps());

  // Once the additional pref is set to false, we should be disabled.
  utils_.pref_service()->SetBoolean(kTestPrefName, false);
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED,
            service->GetStatusFromDeps());

  // The state should not change, even though a signin has occurred.
  service->OnSignInStateChanged(/*has_signed_in=*/true);
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED,
            service->GetStatusFromDeps());
}

TEST_F(RemoteSuggestionsStatusServiceImplTest, EnabledAfterListFolded) {
  auto service = MakeService(/*list_hiding_enabled="*/ true);
  // By default, the articles list should be visible.
  EXPECT_TRUE(utils_.pref_service()->GetBoolean(prefs::kArticlesListVisible));

  // The default test setup is signed out. The service is enabled.
  ASSERT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT,
            service->GetStatusFromDeps());

  // When the user toggles the visibility of articles list in UI off the service
  // should still be enabled until the end of the session.
  utils_.pref_service()->SetBoolean(prefs::kArticlesListVisible, false);
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT,
            service->GetStatusFromDeps());

  // Signin should cause a state change.
  service->OnSignInStateChanged(/*has_signed_in=*/true);
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN,
            service->GetStatusFromDeps());
}

TEST_F(RemoteSuggestionsStatusServiceImplTest, DisabledWhenListFoldedOnStart) {
  utils_.pref_service()->SetBoolean(prefs::kArticlesListVisible, false);
  auto service = MakeService(/*list_hiding_enabled="*/ true);

  // The state should be disabled when starting with no list shown.
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED,
            service->GetStatusFromDeps());

  // The state should not change, even though a signin has occurred.
  service->OnSignInStateChanged(/*has_signed_in=*/true);
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED,
            service->GetStatusFromDeps());
}

TEST_F(RemoteSuggestionsStatusServiceImplTest, EnablingAfterFoldedStart) {
  utils_.pref_service()->SetBoolean(prefs::kArticlesListVisible, false);
  auto service = MakeService(/*list_hiding_enabled="*/ true);

  // The state should be disabled when starting with no list shown.
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED,
            service->GetStatusFromDeps());

  // When the user toggles the visibility of articles list in UI on, the service
  // should get enabled.
  utils_.pref_service()->SetBoolean(prefs::kArticlesListVisible, true);
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT,
            service->GetStatusFromDeps());

  // Signin should cause a state change.
  service->OnSignInStateChanged(/*has_signed_in=*/true);
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN,
            service->GetStatusFromDeps());
}

TEST_F(RemoteSuggestionsStatusServiceImplTest,
       EnablingAfterFoldedStartSignedIn) {
  utils_.pref_service()->SetBoolean(prefs::kArticlesListVisible, false);
  auto service = MakeService(/*list_hiding_enabled="*/ true);

  // Signin should not cause a state change, because UI is not visible.
  service->OnSignInStateChanged(/*has_signed_in=*/true);
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED,
            service->GetStatusFromDeps());

  // When the user toggles the visibility of articles list in UI on, the service
  // should get enabled.
  utils_.pref_service()->SetBoolean(prefs::kArticlesListVisible, true);
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN,
            service->GetStatusFromDeps());
}

}  // namespace ntp_snippets
