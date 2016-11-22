// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/native_app_launcher/native_app_infobar_controller.h"

#include <memory>

#include "base/mac/scoped_nsobject.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_types.h"
#include "ios/chrome/browser/native_app_launcher/native_app_infobar_delegate.h"
#import "ios/chrome/browser/native_app_launcher/native_app_navigation_controller_protocol.h"
#include "testing/platform_test.h"

@interface NativeAppInfoBarController (Testing)
- (void)infoBarButtonDidPress:(UIButton*)button;
- (void)setInfoBarDelegate:(NativeAppInfoBarDelegate*)delegate;
@end

namespace {

class NativeAppInfoBarControllerTest : public PlatformTest {
  class MockNativeAppInfoBarDelegate : public NativeAppInfoBarDelegate {
   public:
    MockNativeAppInfoBarDelegate(NativeAppControllerType type)
        : NativeAppInfoBarDelegate(nil, GURL(), type),
          performedActionCount_(0),
          userAction_(NATIVE_APP_ACTION_COUNT){};

    void CheckIfPerformedActionIsCorrectAndResetCounter() {
      EXPECT_EQ(expectedUserAction_, userAction_);
      EXPECT_EQ(performedActionCount_, 1);
      performedActionCount_ = 0;
    }

    void UserPerformedAction(NativeAppActionType userAction) override {
      userAction_ = userAction;
      performedActionCount_++;
    };

    void SetExpectedUserAction(const NativeAppActionType expectedUserAction) {
      expectedUserAction_ = expectedUserAction;
    };

   private:
    int performedActionCount_;
    NativeAppActionType userAction_;
    NativeAppActionType expectedUserAction_;
  };

  class MockInfobarViewDelegate : public InfoBarViewDelegate {
   public:
    void SetInfoBarTargetHeight(int height) override{};
    void InfoBarDidCancel() override{};
    void InfoBarButtonDidPress(NSUInteger button_id) override{};
  };

 protected:
  void SetUpWithType(NativeAppControllerType type) {
    mockDelegate_.reset(new MockNativeAppInfoBarDelegate(type));
    mockInfoBarViewDelegate_.reset(new MockInfobarViewDelegate());
    infobarController_.reset([[NativeAppInfoBarController alloc]
        initWithDelegate:mockInfoBarViewDelegate_.get()]);
    [infobarController_ setInfoBarDelegate:mockDelegate_.get()];
  };

  // Checks -userPerformedAction: for calls to the delegate's
  // UserPerformedAction member function.
  void ExpectUserPerformedAction(NativeAppActionType performedAction) {
    mockDelegate_->SetExpectedUserAction(performedAction);
    UIButton* mockButton = [UIButton buttonWithType:UIButtonTypeCustom];
    mockButton.tag = performedAction;
    [infobarController_ infoBarButtonDidPress:mockButton];
    mockDelegate_->CheckIfPerformedActionIsCorrectAndResetCounter();
  };

  base::scoped_nsobject<NativeAppInfoBarController> infobarController_;
  std::unique_ptr<MockNativeAppInfoBarDelegate> mockDelegate_;
  std::unique_ptr<MockInfobarViewDelegate> mockInfoBarViewDelegate_;
};

TEST_F(NativeAppInfoBarControllerTest, TestActionsWithInstallInfoBar) {
  SetUpWithType(NATIVE_APP_INSTALLER_CONTROLLER);
  ExpectUserPerformedAction(NATIVE_APP_ACTION_CLICK_INSTALL);
  ExpectUserPerformedAction(NATIVE_APP_ACTION_DISMISS);
}

TEST_F(NativeAppInfoBarControllerTest, TestActionsWithLaunchInfoBar) {
  SetUpWithType(NATIVE_APP_LAUNCHER_CONTROLLER);
  ExpectUserPerformedAction(NATIVE_APP_ACTION_CLICK_LAUNCH);
  ExpectUserPerformedAction(NATIVE_APP_ACTION_DISMISS);
}

TEST_F(NativeAppInfoBarControllerTest, TestActionsWithOpenPolicyInfoBar) {
  SetUpWithType(NATIVE_APP_OPEN_POLICY_CONTROLLER);
  ExpectUserPerformedAction(NATIVE_APP_ACTION_CLICK_ONCE);
  ExpectUserPerformedAction(NATIVE_APP_ACTION_CLICK_ALWAYS);
  ExpectUserPerformedAction(NATIVE_APP_ACTION_DISMISS);
}

}  // namespace
