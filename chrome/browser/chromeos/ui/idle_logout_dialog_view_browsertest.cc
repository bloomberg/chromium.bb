// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/idle_logout_dialog_view.h"

#include "chrome/browser/chromeos/kiosk_mode/mock_kiosk_mode_settings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

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
    dialog->Close();
  }

private:
  KioskModeSettings* mock_settings_;

  DISALLOW_COPY_AND_ASSIGN(MockIdleLogoutSettingsProvider);
};

class IdleLogoutDialogViewTest : public InProcessBrowserTest {
 public:
  IdleLogoutDialogViewTest()
      : mock_provider_(NULL),
        mock_settings_(NULL) {}

  virtual ~IdleLogoutDialogViewTest() {}

  virtual void SetUp() OVERRIDE {
    mock_settings_ = new MockKioskModeSettings();
    mock_provider_ = new MockIdleLogoutSettingsProvider(mock_settings_);
    IdleLogoutDialogView::set_settings_provider(mock_provider_);

    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    delete mock_settings_;
    delete mock_provider_;

    InProcessBrowserTest::TearDown();
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
  MockIdleLogoutSettingsProvider* mock_provider_;
  MockKioskModeSettings* mock_settings_;

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

  IdleLogoutDialogView::current_instance()->Close();
  content::RunAllPendingInMessageLoop();
  ExpectClosedDialog();
}

IN_PROC_BROWSER_TEST_F(IdleLogoutDialogViewTest, ShowDialogAndCloseViewClose) {
  IdleLogoutDialogView::ShowDialog();
  EXPECT_NO_FATAL_FAILURE(ExpectOpenDialog());

  IdleLogoutDialogView::current_instance()->Close();
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
