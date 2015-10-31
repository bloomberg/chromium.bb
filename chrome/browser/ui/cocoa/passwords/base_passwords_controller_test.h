// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_BASE_PASSWORDS_CONTROLLER_TEST_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_BASE_PASSWORDS_CONTROLLER_TEST_H_

#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/passwords/base_passwords_content_view_controller.h"

namespace content {
class WebContents;
}  // namespace content

class ManagePasswordsUIControllerMock;
class ManagePasswordsBubbleModel;

class ManagePasswordsControllerTest : public CocoaProfileTest {
 public:
  ManagePasswordsControllerTest();
  ~ManagePasswordsControllerTest() override;
  void SetUp() override;

  ManagePasswordsUIControllerMock* ui_controller() { return ui_controller_; }
  ManagePasswordsBubbleModel* model();

 private:
  ManagePasswordsUIControllerMock* ui_controller_;
  scoped_ptr<content::WebContents> test_web_contents_;
  scoped_ptr<ManagePasswordsBubbleModel> model_;
};

// Helper delegate for testing the views of the password management bubble.
@interface ContentViewDelegateMock
    : NSObject<ManagePasswordsBubbleContentViewDelegate> {
 @private
  BOOL _dismissed;
}
@property(readonly, nonatomic) BOOL dismissed;
@end

#endif // CHROME_BROWSER_UI_COCOA_PASSWORDS_BASE_PASSWORDS_CONTROLLER_TEST_H_
