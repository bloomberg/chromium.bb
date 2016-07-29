// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_status_service.h"

#include <memory>

#include "components/ntp_snippets/ntp_snippets_test_utils.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace ntp_snippets {

class NTPSnippetsStatusServiceTest : public test::NTPSnippetsTestBase {
 public:
  NTPSnippetsStatusServiceTest() {
    NTPSnippetsStatusService::RegisterProfilePrefs(pref_service()->registry());
  }

  void SetUp() override {
    test::NTPSnippetsTestBase::SetUp();

    service_.reset(
        new NTPSnippetsStatusService(fake_signin_manager(), pref_service()));
  }

 protected:
  NTPSnippetsStatusService* service() { return service_.get(); }

 private:
  std::unique_ptr<NTPSnippetsStatusService> service_;
};

TEST_F(NTPSnippetsStatusServiceTest, SigninStateCompatibility) {
  // The default test setup is signed out.
  EXPECT_EQ(DisabledReason::SIGNED_OUT, service()->GetDisabledReasonFromDeps());

  // Once signed in, we should be in a compatible state.
  fake_signin_manager()->SignIn("foo@bar.com");
  EXPECT_EQ(DisabledReason::NONE, service()->GetDisabledReasonFromDeps());
}

TEST_F(NTPSnippetsStatusServiceTest, DisabledViaPref) {
  // The default test setup is signed out.
  ASSERT_EQ(DisabledReason::SIGNED_OUT, service()->GetDisabledReasonFromDeps());

  // Once the enabled pref is set to false, we should be disabled.
  pref_service()->SetBoolean(prefs::kEnableSnippets, false);
  EXPECT_EQ(DisabledReason::EXPLICITLY_DISABLED,
            service()->GetDisabledReasonFromDeps());

  // The other dependencies shouldn't matter anymore.
  fake_signin_manager()->SignIn("foo@bar.com");
  EXPECT_EQ(DisabledReason::EXPLICITLY_DISABLED,
            service()->GetDisabledReasonFromDeps());
}

}  // namespace ntp_snippets
