// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/password_generation_bubble_controller.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/autofill/password_generator.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "content/public/common/password_form.h"
#include "testing/gtest_mac.h"

using base::HistogramBase;
using base::HistogramSamples;
using base::StatisticsRecorder;

const char kHistogramName[] = "PasswordGeneration.UserActions";

class PasswordGenerationBubbleControllerTest : public CocoaProfileTest {
 public:
  PasswordGenerationBubbleControllerTest()
      : controller_(nil) {}

  static void SetUpTestCase() {
    StatisticsRecorder::Initialize();
  }

  virtual void SetUp() {
    CocoaProfileTest::SetUp();

    generator_.reset(new autofill::PasswordGenerator(20));

    HistogramBase* histogram =
        StatisticsRecorder::FindHistogram(kHistogramName);
    if (histogram)
      original_ = histogram->SnapshotSamples();

    SetUpController();
  }

  PasswordGenerationBubbleController* controller() { return controller_; }

  void SetUpController() {
    content::PasswordForm form;
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

  HistogramSamples* GetHistogramSamples() {
    HistogramBase* histogram =
        StatisticsRecorder::FindHistogram(kHistogramName);
    if (histogram) {
      current_ = histogram->SnapshotSamples();
      if (original_.get())
        current_->Subtract(*original_.get());
    }
    return current_.get();
  }

 protected:
  // Weak.
  PasswordGenerationBubbleController* controller_;

  // Used to determine the histogram changes made just for this specific
  // test run.
  scoped_ptr<HistogramSamples> original_;

  scoped_ptr<HistogramSamples> current_;

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
  [controller() showWindow:nil];

  // Do nothing.
  CloseController();

  HistogramSamples* samples = GetHistogramSamples();
  EXPECT_EQ(1, samples->GetCount(password_generation::IGNORE_FEATURE));
  EXPECT_EQ(0, samples->GetCount(password_generation::ACCEPT_AFTER_EDITING));
  EXPECT_EQ(0, samples->GetCount(
                password_generation::ACCEPT_ORIGINAL_PASSWORD));

  SetUpController();

  // Pretend like the user changed the password and accepted it.
  [controller() controlTextDidChange:nil];
  [controller() fillPassword:nil];
  CloseController();

  samples = GetHistogramSamples();
  EXPECT_EQ(1, samples->GetCount(password_generation::IGNORE_FEATURE));
  EXPECT_EQ(1, samples->GetCount(password_generation::ACCEPT_AFTER_EDITING));
  EXPECT_EQ(0, samples->GetCount(
                password_generation::ACCEPT_ORIGINAL_PASSWORD));

  SetUpController();

  // Just accept the password
  [controller() fillPassword:nil];
  CloseController();

  samples = GetHistogramSamples();
  EXPECT_EQ(1, samples->GetCount(password_generation::IGNORE_FEATURE));
  EXPECT_EQ(1, samples->GetCount(password_generation::ACCEPT_AFTER_EDITING));
  EXPECT_EQ(1, samples->GetCount(
                password_generation::ACCEPT_ORIGINAL_PASSWORD));

}
