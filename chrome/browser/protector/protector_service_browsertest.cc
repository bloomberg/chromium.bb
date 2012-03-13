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
#include "chrome/browser/ui/global_error_bubble_view_base.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

using ::testing::InvokeWithoutArgs;
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
  GlobalError* GetGlobalError(BaseSettingChange* change) {
    std::vector<ProtectorService::Item>::iterator item =
        std::find_if(protector_service_->items_.begin(),
                     protector_service_->items_.end(),
                     ProtectorService::MatchItemByChange(change));
    return item == protector_service_->items_.end() ?
        NULL : item->error.get();
  }

  // Checks that |protector_service_| has an error instance corresponding to
  // |change| and that GlobalErrorService knows about it.
  bool IsGlobalErrorActive(BaseSettingChange* change) {
    GlobalError* error = GetGlobalError(change);
    if (!error)
      return false;
    if (!GlobalErrorServiceFactory::GetForProfile(browser()->profile())->
            GetGlobalErrorByMenuItemCommandID(error->MenuItemCommandID())) {
      return false;
    }
    return protector_service_->IsShowingChange();
  }

  ProtectorService* protector_service_;
  MockSettingChange* mock_change_;
};

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ChangeInitError) {
  // Init fails and causes the change to be ignored.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(false));
  protector_service_->ShowChange(mock_change_);
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowAndDismiss) {
  // Show the change and immediately dismiss it.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  protector_service_->DismissChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowAndApply) {
  // Show the change and apply it.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_CALL(*mock_change_, Apply(browser()));
  protector_service_->ApplyChange(mock_change_, browser());
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowAndApplyManually) {
  // Show the change and apply it, mimicking a button click.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_CALL(*mock_change_, Apply(browser()));
  // Pressing Cancel applies the change.
  GlobalError* error = GetGlobalError(mock_change_);
  ASSERT_TRUE(error);
  error->BubbleViewCancelButtonPressed(browser());
  error->GetBubbleView()->CloseBubbleView();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowAndDiscard) {
  // Show the change and discard it.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_CALL(*mock_change_, Discard(browser()));
  protector_service_->DiscardChange(mock_change_, browser());
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowAndDiscardManually) {
  // Show the change and discard it, mimicking a button click.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_CALL(*mock_change_, Discard(browser()));
  // Pressing Apply discards the change.
  GlobalError* error = GetGlobalError(mock_change_);
  ASSERT_TRUE(error);
  error->BubbleViewAcceptButtonPressed(browser());
  error->GetBubbleView()->CloseBubbleView();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, BubbleClosedInsideApply) {
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));

  GlobalError* error = GetGlobalError(mock_change_);
  ASSERT_TRUE(error);
  GlobalErrorBubbleViewBase* bubble_view = error->GetBubbleView();
  ASSERT_TRUE(bubble_view);
  EXPECT_CALL(*mock_change_, Apply(browser())).WillOnce(InvokeWithoutArgs(
      bubble_view, &GlobalErrorBubbleViewBase::CloseBubbleView));
  // Pressing Cancel applies the change.
  error->BubbleViewCancelButtonPressed(browser());
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowMultipleChangesAndApply) {
  // Show the first change.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));

  // ProtectService will own this change instance as well.
  MockSettingChange* mock_change2 = new NiceMock<MockSettingChange>();
  // Show the second change.
  EXPECT_CALL(*mock_change2, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change2);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_TRUE(IsGlobalErrorActive(mock_change2));

  // Apply the first change, the second should still be active.
  EXPECT_CALL(*mock_change_, Apply(browser()));
  protector_service_->ApplyChange(mock_change_, browser());
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
  EXPECT_TRUE(IsGlobalErrorActive(mock_change2));

  // Finally apply the second change.
  EXPECT_CALL(*mock_change2, Apply(browser()));
  protector_service_->ApplyChange(mock_change2, browser());
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
  EXPECT_FALSE(IsGlobalErrorActive(mock_change2));
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest,
                       ShowMultipleChangesDismissAndApply) {
  // Show the first change.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));

  // ProtectService will own this change instance as well.
  MockSettingChange* mock_change2 = new NiceMock<MockSettingChange>();
  // Show the second change.
  EXPECT_CALL(*mock_change2, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change2);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_TRUE(IsGlobalErrorActive(mock_change2));

  // Dismiss the first change, the second should still be active.
  protector_service_->DismissChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
  EXPECT_TRUE(IsGlobalErrorActive(mock_change2));

  // Finally apply the second change.
  EXPECT_CALL(*mock_change2, Apply(browser()));
  protector_service_->ApplyChange(mock_change2, browser());
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
  EXPECT_FALSE(IsGlobalErrorActive(mock_change2));
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest,
                       ShowMultipleChangesAndApplyManually) {
  // Show the first change.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));

  // The first bubble view has been displayed.
  GlobalError* error = GetGlobalError(mock_change_);
  ASSERT_TRUE(error);
  ASSERT_TRUE(error->HasShownBubbleView());

  // ProtectService will own this change instance as well.
  MockSettingChange* mock_change2 = new NiceMock<MockSettingChange>();
  // Show the second change.
  EXPECT_CALL(*mock_change2, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change2);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_TRUE(IsGlobalErrorActive(mock_change2));

  // The second bubble view hasn't been displayed because the first is still
  // shown.
  GlobalError* error2 = GetGlobalError(mock_change2);
  ASSERT_TRUE(error2);
  EXPECT_FALSE(error2->HasShownBubbleView());

  // Apply the first change, mimicking a button click; the second should still
  // be active.
  EXPECT_CALL(*mock_change_, Apply(browser()));
  error->BubbleViewCancelButtonPressed(browser());
  error->GetBubbleView()->CloseBubbleView();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
  EXPECT_TRUE(IsGlobalErrorActive(mock_change2));

  // Now the second bubble view should be shown.
  ASSERT_TRUE(error2->HasShownBubbleView());

  // Finally apply the second change.
  EXPECT_CALL(*mock_change2, Apply(browser()));
  error2->BubbleViewCancelButtonPressed(browser());
  error2->GetBubbleView()->CloseBubbleView();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
  EXPECT_FALSE(IsGlobalErrorActive(mock_change2));
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest,
                       ShowMultipleChangesAndApplyManuallyBeforeOther) {
  // Show the first change.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));

  // The first bubble view has been displayed.
  GlobalError* error = GetGlobalError(mock_change_);
  ASSERT_TRUE(error);
  ASSERT_TRUE(error->HasShownBubbleView());

  // Apply the first change, mimicking a button click.
  EXPECT_CALL(*mock_change_, Apply(browser()));
  error->BubbleViewCancelButtonPressed(browser());
  error->GetBubbleView()->CloseBubbleView();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));

  // ProtectService will own this change instance as well.
  MockSettingChange* mock_change2 = new NiceMock<MockSettingChange>();
  // Show the second change.
  EXPECT_CALL(*mock_change2, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change2);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change2));

  // The second bubble view has been displayed.
  GlobalError* error2 = GetGlobalError(mock_change2);
  ASSERT_TRUE(error2);
  ASSERT_TRUE(error2->HasShownBubbleView());

  // Finally apply the second change.
  EXPECT_CALL(*mock_change2, Apply(browser()));
  error2->BubbleViewCancelButtonPressed(browser());
  error2->GetBubbleView()->CloseBubbleView();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change2));
}

// TODO(ivankr): Timeout test.

}  // namespace protector
