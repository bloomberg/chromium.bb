// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/protector/mock_setting_change.h"
#include "chrome/browser/protector/protector_service.h"
#include "chrome/browser/protector/protector_service_factory.h"
#include "chrome/browser/protector/settings_change_global_error.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Return;

namespace protector {

class ProtectorServiceTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Make sure protector is enabled.
    command_line->AppendSwitch(switches::kProtector);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    protector_service_ =
        ProtectorServiceFactory::GetForProfile(browser()->profile());
    // ProtectService will own this change instance.
    mock_change_ = new NiceMock<MockSettingChange>();
  }

 protected:
  GlobalError* GetGlobalError(BaseSettingChange* change) {
    for (ProtectorService::Items::iterator item =
             protector_service_->items_.begin();
         item != protector_service_->items_.end(); item++) {
      if (item->change.get() == change)
        return item->error.get();
    }
    return NULL;
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
  EXPECT_FALSE(protector_service_->GetLastChange());
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowAndDismiss) {
  // Show the change and immediately dismiss it.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_EQ(mock_change_, protector_service_->GetLastChange());
  protector_service_->DismissChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
  EXPECT_FALSE(protector_service_->GetLastChange());
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

// ProtectorServiceTest.ShowAndApplyManually is timing out frequently on Win
// bots. http://crbug.com/130590
#if defined(OS_WIN)
#define MAYBE_ShowAndApplyManually DISABLED_ShowAndApplyManually
#else
#define MAYBE_ShowAndApplyManually ShowAndApplyManually
#endif

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, MAYBE_ShowAndApplyManually) {
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
  EXPECT_EQ(mock_change_, protector_service_->GetLastChange());

  // ProtectService will own this change instance as well.
  MockSettingChange* mock_change2 = new NiceMock<MockSettingChange>();
  // Show the second change.
  EXPECT_CALL(*mock_change2, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change2, CanBeMerged()).WillRepeatedly(Return(false));
  protector_service_->ShowChange(mock_change2);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_TRUE(IsGlobalErrorActive(mock_change2));
  EXPECT_EQ(mock_change2, protector_service_->GetLastChange());

  // Apply the first change, the second should still be active.
  EXPECT_CALL(*mock_change_, Apply(browser()));
  protector_service_->ApplyChange(mock_change_, browser());
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
  EXPECT_TRUE(IsGlobalErrorActive(mock_change2));
  EXPECT_EQ(mock_change2, protector_service_->GetLastChange());

  // Finally apply the second change.
  EXPECT_CALL(*mock_change2, Apply(browser()));
  protector_service_->ApplyChange(mock_change2, browser());
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
  EXPECT_FALSE(IsGlobalErrorActive(mock_change2));
  EXPECT_FALSE(protector_service_->GetLastChange());
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
  EXPECT_CALL(*mock_change2, CanBeMerged()).WillRepeatedly(Return(false));
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
  EXPECT_CALL(*mock_change2, CanBeMerged()).WillRepeatedly(Return(false));
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
  EXPECT_CALL(*mock_change2, CanBeMerged()).WillRepeatedly(Return(false));
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

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowMultipleDifferentURLs) {
  GURL url1("http://example.com/");
  GURL url2("http://example.net/");

  // Show the first change with some non-empty URL.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change_, GetNewSettingURL()).WillRepeatedly(Return(url1));
  EXPECT_CALL(*mock_change_, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_EQ(mock_change_, protector_service_->GetLastChange());

  // ProtectService will own this change instance as well.
  MockSettingChange* mock_change2 = new NiceMock<MockSettingChange>();
  // Show the second change with another non-empty URL.
  EXPECT_CALL(*mock_change2, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change2, GetNewSettingURL()).WillRepeatedly(Return(url2));
  EXPECT_CALL(*mock_change2, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change2);
  ui_test_utils::RunAllPendingInMessageLoop();

  // Both changes are shown separately, not composited.
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_TRUE(IsGlobalErrorActive(mock_change2));
  EXPECT_EQ(mock_change2, protector_service_->GetLastChange());

  protector_service_->DismissChange(mock_change_);
  protector_service_->DismissChange(mock_change2);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(protector_service_->GetLastChange());
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowCompositeAndDismiss) {
  GURL url1("http://example.com/");

  // Show the first change.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change_, GetNewSettingURL()).WillRepeatedly(Return(url1));
  EXPECT_CALL(*mock_change_, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_EQ(mock_change_, protector_service_->GetLastChange());

  // The first bubble view has been displayed.
  GlobalError* error = GetGlobalError(mock_change_);
  ASSERT_TRUE(error);
  EXPECT_TRUE(error->HasShownBubbleView());

  // ProtectService will own this change instance as well.
  MockSettingChange* mock_change2 = new NiceMock<MockSettingChange>();
  // Show the second change.
  EXPECT_CALL(*mock_change2, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change2, GetNewSettingURL()).WillRepeatedly(Return(url1));
  EXPECT_CALL(*mock_change2, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change2);
  ui_test_utils::RunAllPendingInMessageLoop();

  // Now ProtectorService should be showing a single composite change.
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
  EXPECT_FALSE(IsGlobalErrorActive(mock_change2));

  BaseSettingChange* composite_change = protector_service_->GetLastChange();
  ASSERT_TRUE(composite_change);
  EXPECT_TRUE(IsGlobalErrorActive(composite_change));

  // The second (composite) bubble view has been displayed.
  GlobalError* error2 = GetGlobalError(composite_change);
  ASSERT_TRUE(error2);
  EXPECT_TRUE(error2->HasShownBubbleView());

  protector_service_->DismissChange(composite_change);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(composite_change));
  EXPECT_FALSE(protector_service_->GetLastChange());

  // Show the third change.
  MockSettingChange* mock_change3 = new NiceMock<MockSettingChange>();
  EXPECT_CALL(*mock_change3, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change3, GetNewSettingURL()).WillRepeatedly(Return(url1));
  EXPECT_CALL(*mock_change3, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change3);
  ui_test_utils::RunAllPendingInMessageLoop();

  // The third change should not be composed with the previous.
  EXPECT_TRUE(IsGlobalErrorActive(mock_change3));
  EXPECT_EQ(mock_change3, protector_service_->GetLastChange());

  protector_service_->DismissChange(mock_change3);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change3));
  EXPECT_FALSE(protector_service_->GetLastChange());
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowCompositeAndOther) {
  GURL url1("http://example.com/");
  GURL url2("http://example.net/");

  // Show the first change.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change_, GetNewSettingURL()).WillRepeatedly(Return(url1));
  EXPECT_CALL(*mock_change_, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_EQ(mock_change_, protector_service_->GetLastChange());

  // ProtectService will own this change instance as well.
  MockSettingChange* mock_change2 = new NiceMock<MockSettingChange>();
  // Show the second change.
  EXPECT_CALL(*mock_change2, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change2, GetNewSettingURL()).WillRepeatedly(Return(url1));
  EXPECT_CALL(*mock_change2, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change2);
  ui_test_utils::RunAllPendingInMessageLoop();

  // Now ProtectorService should be showing a single composite change.
  BaseSettingChange* composite_change = protector_service_->GetLastChange();
  ASSERT_TRUE(composite_change);
  EXPECT_TRUE(IsGlobalErrorActive(composite_change));

  // Show the third change, with the same URL as 1st and 2nd.
  MockSettingChange* mock_change3 = new NiceMock<MockSettingChange>();
  EXPECT_CALL(*mock_change3, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change3, GetNewSettingURL()).WillRepeatedly(Return(url1));
  EXPECT_CALL(*mock_change3, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change3);
  ui_test_utils::RunAllPendingInMessageLoop();

  // The third change should be composed with the previous.
  EXPECT_FALSE(IsGlobalErrorActive(mock_change3));
  EXPECT_EQ(composite_change, protector_service_->GetLastChange());
  EXPECT_TRUE(IsGlobalErrorActive(composite_change));

  // Show the 4th change, now with a different URL.
  MockSettingChange* mock_change4 = new NiceMock<MockSettingChange>();
  EXPECT_CALL(*mock_change4, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change4, GetNewSettingURL()).WillRepeatedly(Return(url2));
  EXPECT_CALL(*mock_change4, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change4);
  ui_test_utils::RunAllPendingInMessageLoop();

  // The 4th change is shown independently.
  EXPECT_TRUE(IsGlobalErrorActive(composite_change));
  EXPECT_TRUE(IsGlobalErrorActive(mock_change4));
  EXPECT_EQ(mock_change4, protector_service_->GetLastChange());

  protector_service_->DismissChange(composite_change);
  protector_service_->DismissChange(mock_change4);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(composite_change));
  EXPECT_FALSE(IsGlobalErrorActive(mock_change4));
  EXPECT_FALSE(protector_service_->GetLastChange());
}

IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, ShowCompositeAndDismissSingle) {
  GURL url1("http://example.com/");
  GURL url2("http://example.net/");

  // Show the first change.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change_, GetNewSettingURL()).WillRepeatedly(Return(url1));
  EXPECT_CALL(*mock_change_, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_EQ(mock_change_, protector_service_->GetLastChange());

  // ProtectService will own this change instance as well.
  MockSettingChange* mock_change2 = new NiceMock<MockSettingChange>();
  // Show the second change.
  EXPECT_CALL(*mock_change2, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change2, GetNewSettingURL()).WillRepeatedly(Return(url1));
  EXPECT_CALL(*mock_change2, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change2);
  ui_test_utils::RunAllPendingInMessageLoop();

  // Now ProtectorService should be showing a single composite change.
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
  EXPECT_FALSE(IsGlobalErrorActive(mock_change2));

  BaseSettingChange* composite_change = protector_service_->GetLastChange();
  ASSERT_TRUE(composite_change);
  EXPECT_TRUE(IsGlobalErrorActive(composite_change));

  // Show the third change with a different URL.
  MockSettingChange* mock_change3 = new NiceMock<MockSettingChange>();
  EXPECT_CALL(*mock_change3, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change3, GetNewSettingURL()).WillRepeatedly(Return(url2));
  EXPECT_CALL(*mock_change3, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change3);
  ui_test_utils::RunAllPendingInMessageLoop();

  // The third change should not be composed with the previous.
  EXPECT_TRUE(IsGlobalErrorActive(mock_change3));
  EXPECT_TRUE(IsGlobalErrorActive(composite_change));
  EXPECT_EQ(mock_change3, protector_service_->GetLastChange());

  // Now dismiss the first change.
  protector_service_->DismissChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();

  // This should effectively dismiss the whole composite change.
  EXPECT_FALSE(IsGlobalErrorActive(composite_change));
  EXPECT_TRUE(IsGlobalErrorActive(mock_change3));
  EXPECT_EQ(mock_change3, protector_service_->GetLastChange());

  protector_service_->DismissChange(mock_change3);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(mock_change3));
  EXPECT_FALSE(protector_service_->GetLastChange());
}

// Verifies that changes with different URLs but same domain are merged.
IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, SameDomainDifferentURLs) {
  GURL url1("http://www.example.com/abc");
  GURL url2("http://example.com/def");

  // Show the first change with some non-empty URL.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change_, GetNewSettingURL()).WillRepeatedly(Return(url1));
  EXPECT_CALL(*mock_change_, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_EQ(mock_change_, protector_service_->GetLastChange());

  // ProtectService will own this change instance as well.
  MockSettingChange* mock_change2 = new NiceMock<MockSettingChange>();
  // Show the second change with another non-empty URL having same domain.
  EXPECT_CALL(*mock_change2, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change2, GetNewSettingURL()).WillRepeatedly(Return(url2));
  EXPECT_CALL(*mock_change2, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change2);
  ui_test_utils::RunAllPendingInMessageLoop();

  // Changes should be merged.
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
  EXPECT_FALSE(IsGlobalErrorActive(mock_change2));

  BaseSettingChange* composite_change = protector_service_->GetLastChange();
  ASSERT_TRUE(composite_change);
  EXPECT_TRUE(IsGlobalErrorActive(composite_change));

  protector_service_->DismissChange(composite_change);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(composite_change));
  EXPECT_FALSE(protector_service_->GetLastChange());
}

// Verifies that changes with different Google URLs are merged.
IN_PROC_BROWSER_TEST_F(ProtectorServiceTest, DifferentGoogleDomains) {
  GURL url1("http://www.google.com/search?q=");
  GURL url2("http://google.ru/search?q=");

  // Show the first change with some non-empty URL.
  EXPECT_CALL(*mock_change_, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change_, GetNewSettingURL()).WillRepeatedly(Return(url1));
  EXPECT_CALL(*mock_change_, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change_);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsGlobalErrorActive(mock_change_));
  EXPECT_EQ(mock_change_, protector_service_->GetLastChange());

  // ProtectService will own this change instance as well.
  MockSettingChange* mock_change2 = new NiceMock<MockSettingChange>();
  // Show the second change with another non-empty URL having same domain.
  EXPECT_CALL(*mock_change2, MockInit(browser()->profile())).
      WillOnce(Return(true));
  EXPECT_CALL(*mock_change2, GetNewSettingURL()).WillRepeatedly(Return(url2));
  EXPECT_CALL(*mock_change2, CanBeMerged()).WillRepeatedly(Return(true));
  protector_service_->ShowChange(mock_change2);
  ui_test_utils::RunAllPendingInMessageLoop();

  // Changes should be merged.
  EXPECT_FALSE(IsGlobalErrorActive(mock_change_));
  EXPECT_FALSE(IsGlobalErrorActive(mock_change2));

  BaseSettingChange* composite_change = protector_service_->GetLastChange();
  ASSERT_TRUE(composite_change);
  EXPECT_TRUE(IsGlobalErrorActive(composite_change));

  protector_service_->DismissChange(composite_change);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsGlobalErrorActive(composite_change));
  EXPECT_FALSE(protector_service_->GetLastChange());
}

// TODO(ivankr): Timeout test.

}  // namespace protector
