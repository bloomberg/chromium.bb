// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/password_generation_bubble_controller.h"

#include "base/logging.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "components/autofill/core/browser/password_generator.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gtest_mac.h"

const char kHistogramName[] = "PasswordGeneration.UserActions";

class PasswordGenerationBubbleControllerTest : public CocoaProfileTest {
 public:
  PasswordGenerationBubbleControllerTest()
      : controller_(nil) {}

  static void SetUpTestCase() {
    base::StatisticsRecorder::Initialize();
  }

  virtual void SetUp() {
    CocoaProfileTest::SetUp();

    generator_.reset(new autofill::PasswordGenerator(20));

    SetUpController();
  }

  PasswordGenerationBubbleController* controller() { return controller_; }

  void SetUpController() {
    autofill::PasswordForm form;
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

  void CloseController() {
    [controller_ close];
    [controller_ windowWillClose:nil];
    controller_ = nil;
  }

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

TEST_F(PasswordGenerationBubbleControllerTest, UMALogging) {
  base::HistogramTester histogram_tester;
  [controller() showWindow:nil];

  // Do nothing.
  CloseController();

  histogram_tester.ExpectUniqueSample(
      kHistogramName, autofill::password_generation::IGNORE_FEATURE, 1);

  SetUpController();

  // Pretend like the user changed the password and accepted it.
  [controller() controlTextDidChange:nil];
  [controller() fillPassword:nil];
  CloseController();

  histogram_tester.ExpectBucketCount(
      kHistogramName, autofill::password_generation::IGNORE_FEATURE, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramName, autofill::password_generation::ACCEPT_AFTER_EDITING, 1);
  histogram_tester.ExpectTotalCount(kHistogramName, 2);

  SetUpController();

  // Just accept the password
  [controller() fillPassword:nil];
  CloseController();

  histogram_tester.ExpectBucketCount(
      kHistogramName, autofill::password_generation::IGNORE_FEATURE, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramName, autofill::password_generation::ACCEPT_AFTER_EDITING, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramName,
      autofill::password_generation::ACCEPT_ORIGINAL_PASSWORD,
      1);
  histogram_tester.ExpectTotalCount(kHistogramName, 3);
}
