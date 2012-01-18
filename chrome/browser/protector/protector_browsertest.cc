// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/protector/mock_setting_change.h"
#include "chrome/browser/protector/protector.h"
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

class ProtectorTest : public InProcessBrowserTest {
 public:
  virtual void SetUpOnMainThread() {
    // Protect owns the change and deletes himself.
    protector_ = new Protector(browser()->profile());
    mock_change_ = new NiceMock<MockSettingChange>();
  }

 protected:
  GlobalError* GetGlobalError() {
    return protector_->error_.get();
  }

  void ExpectGlobalErrorActive(bool active) {
    GlobalError* error =
        GlobalErrorServiceFactory::GetForProfile(browser()->profile())->
            GetGlobalErrorByMenuItemCommandID(IDC_SHOW_SETTINGS_CHANGES);
    EXPECT_EQ(active, static_cast<bool>(error));
  }

  Protector* protector_;
  MockSettingChange* mock_change_;
};

IN_PROC_BROWSER_TEST_F(ProtectorTest, ChangeInitError) {
  EXPECT_CALL(*mock_change_, MockInit(protector_)).WillOnce(Return(false));
  protector_->ShowChange(mock_change_);
  ExpectGlobalErrorActive(false);
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(false);
}

IN_PROC_BROWSER_TEST_F(ProtectorTest, ShowAndDismiss) {
  EXPECT_CALL(*mock_change_, MockInit(protector_)).WillOnce(Return(true));
  protector_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  ASSERT_TRUE(GetGlobalError());
  ExpectGlobalErrorActive(true);
  protector_->DismissChange();
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(false);
}

IN_PROC_BROWSER_TEST_F(ProtectorTest, ShowAndApply) {
  EXPECT_CALL(*mock_change_, MockInit(protector_)).WillOnce(Return(true));
  protector_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  GlobalError* error = GetGlobalError();
  ASSERT_TRUE(error);
  ExpectGlobalErrorActive(true);
  EXPECT_CALL(*mock_change_, Apply());
  // Pressing Cancel applies the change.
  error->BubbleViewCancelButtonPressed();
  error->BubbleViewDidClose();
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(false);
}

IN_PROC_BROWSER_TEST_F(ProtectorTest, ShowAndDiscard) {
  EXPECT_CALL(*mock_change_, MockInit(protector_)).WillOnce(Return(true));
  protector_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  GlobalError* error = GetGlobalError();
  ASSERT_TRUE(error);
  ExpectGlobalErrorActive(true);
  EXPECT_CALL(*mock_change_, Discard());
  // Pressing Apply discards the change.
  error->BubbleViewAcceptButtonPressed();
  error->BubbleViewDidClose();
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(false);
}

IN_PROC_BROWSER_TEST_F(ProtectorTest, BubbleClosedInsideApply) {
  EXPECT_CALL(*mock_change_, MockInit(protector_)).WillOnce(Return(true));
  protector_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  GlobalError* error = GetGlobalError();
  ASSERT_TRUE(error);
  ExpectGlobalErrorActive(true);
  EXPECT_CALL(*mock_change_, Apply()).
      WillOnce(Invoke(error, &GlobalError::BubbleViewDidClose));
  // Pressing Cancel applies the change.
  error->BubbleViewCancelButtonPressed();
  ui_test_utils::RunAllPendingInMessageLoop();
  ExpectGlobalErrorActive(false);
}

// TODO(ivankr): Timeout test.

}  // namespace protector
