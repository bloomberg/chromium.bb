// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_TEST_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_TEST_H_

#include "base/metrics/histogram_samples.h"
#include "base/test/statistics_delta_reader.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gtest/include/gtest/gtest.h"

class ManagePasswordsUIControllerMock;
class ManagePasswordsIconView;

// Test class for the various password management view bits and pieces. Sets
// up a ManagePasswordsUIControllerMock, and provides some helper methods
// to poke at the bubble, icon, and controller's state.
class ManagePasswordsViewTest : public InProcessBrowserTest {
 public:
  ManagePasswordsViewTest() {}

  // InProcessBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE;

  // Get the mock UI controller for the current WebContents.
  ManagePasswordsUIControllerMock* controller();

  // Get the icon view for the current WebContents.
  ManagePasswordsIconView* view();

  // Execute the browser command to open the manage passwords bubble.
  void ExecuteManagePasswordsCommand();

  // Put the controller, icon, and bubble into a managing-password state.
  void SetupManagingPasswords();

  // Put the controller, icon, and bubble into the confirmation state.
  void SetupAutomaticPassword();

  // Put the controller, icon, and bubble into a pending-password state.
  void SetupPendingPassword();

  // Put the controller, icon, and bubble into a blacklisted state.
  void SetupBlackistedPassword();

  // Get samples for |histogram|.
  base::HistogramSamples* GetSamples(const char* histogram);

  autofill::PasswordForm* test_form() { return &test_form_; }

 private:
  autofill::PasswordForm test_form_;
  base::StatisticsDeltaReader statistics_reader_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsViewTest);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_TEST_H_
