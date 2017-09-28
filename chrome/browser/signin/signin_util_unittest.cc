// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"

class SigninUtilTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    signin_util::ResetForceSigninForTesting();

    prefs_.reset(new TestingPrefServiceSimple());
  }

  void TearDown() override {
    signin_util::ResetForceSigninForTesting();
    BrowserWithTestWindowTest::TearDown();
  }

  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

TEST_F(SigninUtilTest, GetForceSigninPolicy) {
  ASSERT_FALSE(signin_util::IsForceSigninEnabled());
  chrome::RegisterLocalState(prefs_->registry());
  TestingBrowserProcess::GetGlobal()->SetLocalState(prefs_.get());

  g_browser_process->local_state()->SetBoolean(prefs::kForceBrowserSignin,
                                               true);
  ASSERT_TRUE(signin_util::IsForceSigninEnabled());
  g_browser_process->local_state()->SetBoolean(prefs::kForceBrowserSignin,
                                               false);
  ASSERT_TRUE(signin_util::IsForceSigninEnabled());

  TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
}
