// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_status_service_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/variations_params_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

class RemoteSuggestionsStatusServiceImplTest : public ::testing::Test {
 public:
  RemoteSuggestionsStatusServiceImplTest() {
    RemoteSuggestionsStatusServiceImpl::RegisterProfilePrefs(
        utils_.pref_service()->registry());
  }

  std::unique_ptr<RemoteSuggestionsStatusServiceImpl> MakeService() {
    return base::MakeUnique<RemoteSuggestionsStatusServiceImpl>(
        utils_.fake_signin_manager(), utils_.pref_service(), std::string());
  }

 protected:
  test::RemoteSuggestionsTestUtils utils_;
  variations::testing::VariationParamsManager params_manager_;
};

TEST_F(RemoteSuggestionsStatusServiceImplTest, NoSigninNeeded) {
  auto service = MakeService();

  // By default, no signin is required.
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT,
            service->GetStatusFromDeps());

// One can still sign in.
#if defined(OS_CHROMEOS)
  utils_.fake_signin_manager()->SignIn("foo@bar.com");
#else
  utils_.fake_signin_manager()->SignIn("foo@bar.com", "user", "pass");
#endif
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN,
            service->GetStatusFromDeps());
}

TEST_F(RemoteSuggestionsStatusServiceImplTest, DisabledViaPref) {
  auto service = MakeService();

  // The default test setup is signed out. The service is enabled.
  ASSERT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT,
            service->GetStatusFromDeps());

  // Once the enabled pref is set to false, we should be disabled.
  utils_.pref_service()->SetBoolean(prefs::kEnableSnippets, false);
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED,
            service->GetStatusFromDeps());

// The other dependencies shouldn't matter anymore.
#if defined(OS_CHROMEOS)
  utils_.fake_signin_manager()->SignIn("foo@bar.com");
#else
  utils_.fake_signin_manager()->SignIn("foo@bar.com", "user", "pass");
#endif
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED,
            service->GetStatusFromDeps());
}

}  // namespace ntp_snippets
