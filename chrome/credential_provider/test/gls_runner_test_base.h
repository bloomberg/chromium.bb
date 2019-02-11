// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_TEST_GLS_RUNNER_TEST_BASE_H_
#define CHROME_CREDENTIAL_PROVIDER_TEST_GLS_RUNNER_TEST_BASE_H_

#include "base/test/test_reg_util_win.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/fake_gls_run_helper.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

// Helper class used to run and wait for a fake GLS process to validate the
// functionality of a GCPW credential.
class GlsRunnerTestBase : public ::testing::Test {
 protected:
  GlsRunnerTestBase();
  ~GlsRunnerTestBase() override;

  void SetUp() override;
  FakeGlsRunHelper* run_helper() { return &run_helper_; }

  FakeOSUserManager* fake_os_user_manager() { return &fake_os_user_manager_; }

 private:
  FakeOSProcessManager fake_os_process_manager_;
  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
  FakeScopedUserProfileFactory fake_scoped_user_profile_factory_;
  registry_util::RegistryOverrideManager registry_override_;
  FakeGlsRunHelper run_helper_;
};

}  // namespace testing

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_TEST_GLS_RUNNER_TEST_BASE_H_
