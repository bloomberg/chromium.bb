// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/password_manager/auto_signin_first_run_dialog_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class AutoSigninFirstRunDialogAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutoSigninFirstRunDialogAndroidTest() {}
  ~AutoSigninFirstRunDialogAndroidTest() override {}

  PrefService* prefs();

 protected:
  scoped_ptr<AutoSigninFirstRunDialogAndroid> CreateDialog();

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoSigninFirstRunDialogAndroidTest);
};

scoped_ptr<AutoSigninFirstRunDialogAndroid>
AutoSigninFirstRunDialogAndroidTest::CreateDialog() {
  scoped_ptr<AutoSigninFirstRunDialogAndroid> dialog(
      new AutoSigninFirstRunDialogAndroid(web_contents()));
  return dialog;
}

PrefService* AutoSigninFirstRunDialogAndroidTest::prefs() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return profile->GetPrefs();
}

TEST_F(AutoSigninFirstRunDialogAndroidTest,
       CheckResetOfPrefAfterFirstRunMessageWasShown) {
  prefs()->SetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, false);
  scoped_ptr<AutoSigninFirstRunDialogAndroid> dialog(CreateDialog());
  dialog.reset();
  EXPECT_TRUE(prefs()->GetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown));
}
