// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_status_service.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/test_utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/variations/variations_params_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

class RemoteSuggestionsStatusServiceTest : public ::testing::Test {
 public:
  RemoteSuggestionsStatusServiceTest() {
    RemoteSuggestionsStatusService::RegisterProfilePrefs(
        utils_.pref_service()->registry());
  }

  std::unique_ptr<RemoteSuggestionsStatusService> MakeService() {
    return base::MakeUnique<RemoteSuggestionsStatusService>(
        utils_.fake_signin_manager(), utils_.pref_service());
  }

 protected:
  test::RemoteSuggestionsTestUtils utils_;
  variations::testing::VariationParamsManager params_manager_;
};

TEST_F(RemoteSuggestionsStatusServiceTest, NoSigninNeeded) {
  auto service = MakeService();

  // By default, no signin is required.
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT,
            service->GetStatusFromDeps());

  // One can still sign in.
  utils_.fake_signin_manager()->SignIn("foo@bar.com");
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN,
            service->GetStatusFromDeps());
}

TEST_F(RemoteSuggestionsStatusServiceTest, DisabledViaPref) {
  auto service = MakeService();

  // The default test setup is signed out. The service is enabled.
  ASSERT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT,
            service->GetStatusFromDeps());

  // Once the enabled pref is set to false, we should be disabled.
  utils_.pref_service()->SetBoolean(prefs::kEnableSnippets, false);
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED,
            service->GetStatusFromDeps());

  // The other dependencies shouldn't matter anymore.
  utils_.fake_signin_manager()->SignIn("foo@bar.com");
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED,
            service->GetStatusFromDeps());
}

}  // namespace ntp_snippets
