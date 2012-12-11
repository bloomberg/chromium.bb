// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/password_generation_bubble_controller.h"

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/autofill/password_generator.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "content/public/common/password_form.h"
#include "testing/gtest_mac.h"

class PasswordGenerationBubbleControllerTest : public CocoaProfileTest {
 public:
  PasswordGenerationBubbleControllerTest()
      : controller_(nil) {}

  virtual void SetUp() {
    CocoaTest::SetUp();

    content::PasswordForm form;
    generator_.reset(new autofill::PasswordGenerator(20));
    NSRect frame = [test_window() frame];
    NSPoint point = NSMakePoint(NSMidX(frame), NSMidY(frame));

    // |controller_| is self deleting.
    controller_ = [[PasswordGenerationBubbleController alloc]
                    initWithWindow:test_window()
                        anchoredAt:point
                    renderViewHost:nil
                   passwordManager:nil
                    usingGenerator:generator_.get()
                           forForm:form];
  }

  PasswordGenerationBubbleController* controller() { return controller_; }

 protected:
  // Weak.
  PasswordGenerationBubbleController* controller_;

  scoped_ptr<autofill::PasswordGenerator> generator_;
};

TEST_F(PasswordGenerationBubbleControllerTest, Regenerate) {
  [controller() showWindow:nil];

  PasswordGenerationTextField* textfield = controller().textField;
  // Grab the starting password value.
  NSString* before = [textfield stringValue];

  // Click on the regenerate icon.
  [textfield simulateIconClick];

  // Make sure that the password has changed. Technically this will fail
  // about once every 1e28 times, but not something we really need to worry
  // about.
  NSString* after = [textfield stringValue];
  EXPECT_FALSE([before isEqualToString:after]);
}
