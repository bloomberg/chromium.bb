// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/password_manager/auto_signin_first_run_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestAutoSigninFirstRunInfoBarDelegate
    : public AutoSigninFirstRunInfoBarDelegate {
 public:
  explicit TestAutoSigninFirstRunInfoBarDelegate(
      content::WebContents* web_contents)
      : AutoSigninFirstRunInfoBarDelegate(
            web_contents,
            true /* is_smartlock_branding_enabled */,
            base::string16()) {}

  ~TestAutoSigninFirstRunInfoBarDelegate() override {}
};

}  // namespace

class AutoSigninFirstRunInfoBarDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutoSigninFirstRunInfoBarDelegateTest() {}
  ~AutoSigninFirstRunInfoBarDelegateTest() override {}

  PrefService* prefs();

 protected:
  scoped_ptr<ConfirmInfoBarDelegate> CreateDelegate();

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoSigninFirstRunInfoBarDelegateTest);
};

scoped_ptr<ConfirmInfoBarDelegate>
AutoSigninFirstRunInfoBarDelegateTest::CreateDelegate() {
  scoped_ptr<ConfirmInfoBarDelegate> delegate(
      new TestAutoSigninFirstRunInfoBarDelegate(web_contents()));
  return delegate;
}

PrefService* AutoSigninFirstRunInfoBarDelegateTest::prefs() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return profile->GetPrefs();
}

TEST_F(AutoSigninFirstRunInfoBarDelegateTest,
       CheckResetOfPrefAfterFirstRunMessageWasShownOnCancel) {
  prefs()->SetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, false);
  scoped_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate());
  EXPECT_TRUE(infobar->Cancel());
  infobar.reset();
  EXPECT_TRUE(prefs()->GetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown));
}

TEST_F(AutoSigninFirstRunInfoBarDelegateTest,
       CheckResetOfPrefAfterFirstRunMessageWasShownOnAccept) {
  prefs()->SetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, false);
  scoped_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate());
  EXPECT_TRUE(infobar->Accept());
  infobar.reset();
  EXPECT_TRUE(prefs()->GetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown));
}
