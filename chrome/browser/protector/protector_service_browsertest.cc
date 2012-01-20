// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/protector/mock_setting_change.h"
#include "chrome/browser/protector/protector_service.h"
#include "chrome/browser/protector/protector_service_factory.h"
#include "chrome/browser/protector/settings_change_global_error.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace protector {

class ProtectorServiceTest : public InProcessBrowserTest {
 public:
  virtual void SetUpOnMainThread() {
    protector_service_ =
        ProtectorServiceFactory::GetForProfile(browser()->profile());
    // ProtectService will own this change instance.
    mock_change_ = new NiceMock<MockSettingChange>();
  }

 protected:
  GlobalError* GetGlobalError() {
    return protector_service_->error_.get();
  }

  void ExpectGlobalErrorActive(bool active) {
    GlobalError* error =
        GlobalErrorServiceFactory::GetForProfile(browser()->profile())->
            GetGlobalErrorByMenuItemCommandID(IDC_SHOW_SETTINGS_CHANGES);
    EXPECT_EQ(active, error != NULL);
    EXPECT_EQ(active, GetGlobalError() != NULL);
    EXPECT_EQ(active, protector_service_->IsShowingChange());
  }

  ProtectorService* protector_service_;
  MockSettingChange* mock_change_;
};

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ChangeInitError) {
  // Init fails and causes the change to be ignored.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(false));
  protector_service_->ShowChange(mock_change_);
  ExpectGlobalErrorActive(false);
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(false);
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowAndDismiss) {
  // Show the change and immediately dismiss it.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(true);
  protector_service_->DismissChange();
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(false);
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowAndApply) {
  // Show the change and apply it.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(true);
  EXPECT_CALL(*mock_change_, Apply());
  protector_service_->ApplyChange();
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(false);
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowAndApplyManually) {
  // Show the change and apply it, mimicking a button click.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(true);
  EXPECT_CALL(*mock_change_, Apply());
  // Pressing Cancel applies the change.
  GlobalError* error = GetGlobalError();
  error->BubbleViewCancelButtonPressed();
  error->BubbleViewDidClose();
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(false);
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowAndDiscard) {
  // Show the change and discard it.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(true);
  EXPECT_CALL(*mock_change_, Discard());
  protector_service_->DiscardChange();
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(false);
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowAndDiscardManually) {
  // Show the change and discard it, mimicking a button click.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(true);
  EXPECT_CALL(*mock_change_, Discard());
  // Pressing Apply discards the change.
  GlobalError* error = GetGlobalError();
  error->BubbleViewAcceptButtonPressed();
  error->BubbleViewDidClose();
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(false);
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, BubbleClosedInsideApply) {
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(true);
  GlobalError* error = GetGlobalError();
  EXPECT_CALL(*mock_change_, Apply()).
      WillOnce(Invoke(error, &GlobalError::BubbleViewDidClose));
  // Pressing Cancel applies the change.
  error->BubbleViewCancelButtonPressed();
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(false);
}

// TODO(ivankr): Timeout test.

}  // namespace protector
