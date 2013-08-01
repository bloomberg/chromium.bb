// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/idle_logout_dialog_view.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/kiosk_mode/mock_kiosk_mode_settings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

class MockIdleLogoutSettingsProvider : public IdleLogoutSettingsProvider {
public:
  explicit MockIdleLogoutSettingsProvider(KioskModeSettings* mock_settings)
      : mock_settings_(mock_settings) {}

  virtual base::TimeDelta GetCountdownUpdateInterval() OVERRIDE {
    return base::TimeDelta::FromMilliseconds(0);
  }

  virtual KioskModeSettings* GetKioskModeSettings() OVERRIDE {
    return mock_settings_;
  }

  virtual void LogoutCurrentUser(IdleLogoutDialogView* dialog) OVERRIDE {
    dialog->GetWidget()->Close();
  }

private:
  KioskModeSettings* mock_settings_;

  DISALLOW_COPY_AND_ASSIGN(MockIdleLogoutSettingsProvider);
};

class IdleLogoutDialogViewTest : public InProcessBrowserTest {
 public:
  IdleLogoutDialogViewTest() {}

  virtual ~IdleLogoutDialogViewTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    mock_settings_.reset(new MockKioskModeSettings());
    mock_provider_.reset(
        new MockIdleLogoutSettingsProvider(mock_settings_.get()));
    IdleLogoutDialogView::set_settings_provider(mock_provider_.get());
  }

  void ExpectOpenDialog() {
    IdleLogoutDialogView* dialog = IdleLogoutDialogView::current_instance();
    ASSERT_TRUE(dialog != NULL);
    EXPECT_TRUE(dialog->visible());
  }

  void ExpectClosedDialog() {
    EXPECT_TRUE(IdleLogoutDialogView::current_instance() == NULL);
  }

 private:
  scoped_ptr<MockIdleLogoutSettingsProvider> mock_provider_;
  scoped_ptr<MockKioskModeSettings> mock_settings_;

  DISALLOW_COPY_AND_ASSIGN(IdleLogoutDialogViewTest);
};

IN_PROC_BROWSER_TEST_F(IdleLogoutDialogViewTest, ShowDialogAndClose) {
  IdleLogoutDialogView::ShowDialog();
  EXPECT_NO_FATAL_FAILURE(ExpectOpenDialog());

  IdleLogoutDialogView::CloseDialog();
  ExpectClosedDialog();
}

IN_PROC_BROWSER_TEST_F(IdleLogoutDialogViewTest, ShowDialogAndCloseView) {
  IdleLogoutDialogView::ShowDialog();
  EXPECT_NO_FATAL_FAILURE(ExpectOpenDialog());

  IdleLogoutDialogView::CloseDialog();
  content::RunAllPendingInMessageLoop();
  ExpectClosedDialog();
}

IN_PROC_BROWSER_TEST_F(IdleLogoutDialogViewTest, ShowDialogAndCloseViewClose) {
  IdleLogoutDialogView::ShowDialog();
  EXPECT_NO_FATAL_FAILURE(ExpectOpenDialog());

  IdleLogoutDialogView::CloseDialog();
  content::RunAllPendingInMessageLoop();
  IdleLogoutDialogView::CloseDialog();

  ExpectClosedDialog();
}

IN_PROC_BROWSER_TEST_F(IdleLogoutDialogViewTest,
                       OutOfOrderMultipleShowDialogAndClose) {
  IdleLogoutDialogView::CloseDialog();
  ExpectClosedDialog();

  IdleLogoutDialogView::ShowDialog();
  EXPECT_NO_FATAL_FAILURE(ExpectOpenDialog());
  IdleLogoutDialogView::ShowDialog();
  EXPECT_NO_FATAL_FAILURE(ExpectOpenDialog());

  IdleLogoutDialogView::CloseDialog();
  ExpectClosedDialog();
  IdleLogoutDialogView::CloseDialog();
  ExpectClosedDialog();
}

IN_PROC_BROWSER_TEST_F(IdleLogoutDialogViewTest,
                       ShowDialogAndFinishCountdown) {
  IdleLogoutDialogView::ShowDialog();
  EXPECT_NO_FATAL_FAILURE(ExpectOpenDialog());

  content::RunAllPendingInMessageLoop();
  ExpectClosedDialog();
}

}  // namespace chromeos
