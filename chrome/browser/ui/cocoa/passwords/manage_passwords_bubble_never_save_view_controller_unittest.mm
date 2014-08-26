// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_never_save_view_controller.h"

#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_never_save_view_controller.h"
#include "chrome/browser/ui/cocoa/passwords/manage_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "components/password_manager/core/common/password_manager_ui.h"

// Helper delegate for testing the never save view of the password management
// bubble.
@interface ManagePasswordsBubbleNeverSaveViewTestDelegate
    : NSObject<ManagePasswordsBubbleNeverSaveViewDelegate> {
  BOOL dismissed_;
  BOOL cancelledNeverSave_;
}
@property(readonly) BOOL dismissed;
@property(readonly) BOOL cancelledNeverSave;
@end

@implementation ManagePasswordsBubbleNeverSaveViewTestDelegate

@synthesize dismissed = dismissed_;
@synthesize cancelledNeverSave = cancelledNeverSave_;

- (void)viewShouldDismiss {
  dismissed_ = YES;
}

- (void)neverSavePasswordCancelled {
  cancelledNeverSave_ = YES;
}

@end

namespace {

// Tests for the never save view of the password management bubble.
class ManagePasswordsBubbleNeverSaveViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManagePasswordsBubbleNeverSaveViewControllerTest() : controller_(nil) {}

  virtual void SetUp() OVERRIDE {
    ManagePasswordsControllerTest::SetUp();
    delegate_.reset(
        [[ManagePasswordsBubbleNeverSaveViewTestDelegate alloc] init]);
    ui_controller()->SetState(password_manager::ui::PENDING_PASSWORD_STATE);
  }

  ManagePasswordsBubbleNeverSaveViewTestDelegate* delegate() {
    return delegate_.get();
  }

  ManagePasswordsBubbleNeverSaveViewController* controller() {
    if (!controller_) {
      controller_.reset([[ManagePasswordsBubbleNeverSaveViewController alloc]
          initWithModel:model()
               delegate:delegate()]);
      [controller_ loadView];
    }
    return controller_.get();
  }

 private:
  base::scoped_nsobject<ManagePasswordsBubbleNeverSaveViewController>
      controller_;
  base::scoped_nsobject<ManagePasswordsBubbleNeverSaveViewTestDelegate>
      delegate_;
};

TEST_F(ManagePasswordsBubbleNeverSaveViewControllerTest,
       ShouldNotifyDelegateWhenUndoClicked) {
  NSButton* undoButton = controller().undoButton;
  [undoButton performClick:nil];
  EXPECT_TRUE(delegate().cancelledNeverSave);
}

TEST_F(ManagePasswordsBubbleNeverSaveViewControllerTest,
       ShouldDismissAndNeverSaveWhenConfirmClicked) {
  NSButton* confirmButton = controller().confirmButton;
  [confirmButton performClick:nil];
  EXPECT_TRUE(delegate().dismissed);
  EXPECT_TRUE(ui_controller()->never_saved_password());
}

}  // namespace
