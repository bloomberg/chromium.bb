// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/account_chooser_view_controller.h"

#include <Carbon/Carbon.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/passwords/autosignin_prompt_view_controller.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/browser/ui/passwords/password_dialog_controller_mock.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/events/test/cocoa_test_event_utils.h"


namespace {

constexpr char kDialogTitle[] = "Auto signin";
constexpr char kDialogText[] = "bla bla";

// Tests for the autosignin dialog view.
class AutoSigninPromptViewControllerTest : public PasswordPromptBridgeInterface,
                                           public CocoaTest {
 public:
  PasswordDialogControllerMock& dialog_controller() {
    return dialog_controller_;
  }

  AutoSigninPromptViewController* view_controller() {
    return view_controller_.get();
  }

  void SetUpAutosigninFirstRun();

  MOCK_METHOD0(OnPerformClose, void());

  // PasswordPromptBridgeInterface:
  void PerformClose() override;
  PasswordDialogController* GetDialogController() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override;

 private:
  PasswordDialogControllerMock dialog_controller_;
  base::scoped_nsobject<AutoSigninPromptViewController> view_controller_;
};

void AutoSigninPromptViewControllerTest::SetUpAutosigninFirstRun() {
  view_controller_.reset([[AutoSigninPromptViewController alloc]
                              initWithBridge:this]);
  EXPECT_CALL(dialog_controller_, GetAutoSigninPromoTitle())
      .WillOnce(testing::Return(base::ASCIIToUTF16(kDialogTitle)));
  EXPECT_CALL(dialog_controller_, GetAutoSigninText())
      .WillOnce(testing::Return(std::make_pair(base::ASCIIToUTF16(kDialogText),
                                               gfx::Range(0, 5))));
  [view_controller_ view];
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&dialog_controller_));
}

void AutoSigninPromptViewControllerTest::PerformClose() {
  view_controller_.reset();
  OnPerformClose();
}

PasswordDialogController*
AutoSigninPromptViewControllerTest::GetDialogController() {
  return &dialog_controller_;
}

scoped_refptr<network::SharedURLLoaderFactory>
AutoSigninPromptViewControllerTest::GetURLLoaderFactory() const {
  NOTREACHED();
  return nullptr;
}

TEST_F(AutoSigninPromptViewControllerTest, ClickOk) {
  SetUpAutosigninFirstRun();
  EXPECT_CALL(dialog_controller(), OnAutoSigninOK());
  [view_controller().okButton performClick:nil];
}

TEST_F(AutoSigninPromptViewControllerTest, ClickTurnOff) {
  SetUpAutosigninFirstRun();
  EXPECT_CALL(dialog_controller(), OnAutoSigninTurnOff());
  [view_controller().turnOffButton performClick:nil];
}

TEST_F(AutoSigninPromptViewControllerTest, ClickContentLink) {
  SetUpAutosigninFirstRun();
  EXPECT_CALL(dialog_controller(), OnSmartLockLinkClicked());
  [view_controller().contentText clickedOnLink:@""
                                       atIndex:0];
}

TEST_F(AutoSigninPromptViewControllerTest, ClosePromptAndHandleClick) {
  // A user may press mouse down, some navigation closes the dialog, mouse up
  // still sends the action. The view should not crash.
  SetUpAutosigninFirstRun();
  [view_controller() setBridge:nil];
  [view_controller().contentText clickedOnLink:@"" atIndex:0];
  [view_controller().okButton performClick:nil];
  [view_controller().turnOffButton performClick:nil];
}

TEST_F(AutoSigninPromptViewControllerTest, CloseOnEsc) {
  SetUpAutosigninFirstRun();
  EXPECT_CALL(*this, OnPerformClose());
  [[view_controller() view]
      performKeyEquivalent:cocoa_test_event_utils::KeyEventWithKeyCode(
                               kVK_Escape, '\e', NSKeyDown, 0)];
}

}  // namespace
