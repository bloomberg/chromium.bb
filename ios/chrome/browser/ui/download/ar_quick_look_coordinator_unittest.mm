// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/ar_quick_look_coordinator.h"

#import <QuickLook/QuickLook.h>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "ios/chrome/browser/download/download_test_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

namespace {

// Returns the absolute path for the test file in the test data directory.
base::FilePath GetTestFilePath() {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_MODULE, &file_path);
  file_path = file_path.Append(FILE_PATH_LITERAL(testing::kUsdzFilePath));
  return file_path;
}

}  // namespace

class ARQuickLookCoordinatorTest : public PlatformTest {
 protected:
  ARQuickLookCoordinatorTest()
      : base_view_controller_([[UIViewController alloc] init]),
        coordinator_([[ARQuickLookCoordinator alloc]
            initWithBaseViewController:base_view_controller_]) {
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
  }

  UIViewController* base_view_controller_;
  ARQuickLookCoordinator* coordinator_;
  ScopedKeyWindow scoped_key_window_;
  base::HistogramTester histogram_tester_;
};

// Tests presenting a valid USDZ file.
TEST_F(ARQuickLookCoordinatorTest, ValidUSDZFile) {
  base::FilePath path = GetTestFilePath();
  NSURL* fileURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];
  [coordinator_ ARQuickLookTabHelper:nil
      didFinishDowloadingFileWithURL:fileURL];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [QLPreviewController class];
  }));

  [coordinator_ stop];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return base_view_controller_.presentedViewController == nil;
  }));

  histogram_tester_.ExpectUniqueSample(
      kIOSPresentQLPreviewControllerHistogram,
      static_cast<base::HistogramBase::Sample>(
          PresentQLPreviewController::kSuccessful),
      1);
}

// Tests attempting to present an invalid USDZ file.
TEST_F(ARQuickLookCoordinatorTest, InvalidUSDZFile) {
  [coordinator_ ARQuickLookTabHelper:nil didFinishDowloadingFileWithURL:nil];

  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [QLPreviewController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kIOSPresentQLPreviewControllerHistogram,
      static_cast<base::HistogramBase::Sample>(
          PresentQLPreviewController::kInvalidFile),
      1);
}

// Tests presenting multiple USDZ files. The subsequent attempts get ignored.
TEST_F(ARQuickLookCoordinatorTest, MultipleValidUSDZFiles) {
  base::FilePath path = GetTestFilePath();
  NSURL* fileURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];
  [coordinator_ ARQuickLookTabHelper:nil
      didFinishDowloadingFileWithURL:fileURL];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [QLPreviewController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kIOSPresentQLPreviewControllerHistogram,
      static_cast<base::HistogramBase::Sample>(
          PresentQLPreviewController::kSuccessful),
      1);

  // Attempt to present QLPreviewController again.
  UIViewController* presented_view_controller =
      base_view_controller_.presentedViewController;

  [coordinator_ ARQuickLookTabHelper:nil
      didFinishDowloadingFileWithURL:fileURL];

  // The attempt is ignored.
  EXPECT_EQ(presented_view_controller,
            base_view_controller_.presentedViewController);

  histogram_tester_.ExpectBucketCount(
      kIOSPresentQLPreviewControllerHistogram,
      static_cast<base::HistogramBase::Sample>(
          PresentQLPreviewController::kAnotherQLPreviewControllerIsPresented),
      1);

  [coordinator_ stop];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return base_view_controller_.presentedViewController == nil;
  }));
}

// Tests presenting a valid USDZ file while a view controller is presented.
TEST_F(ARQuickLookCoordinatorTest, AnotherViewControllerIsPresented) {
  // Present a view controller.
  UIViewController* presented_view_controller = [[UIViewController alloc] init];
  [base_view_controller_ presentViewController:presented_view_controller
                                      animated:YES
                                    completion:nil];
  EXPECT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForUIElementTimeout, ^{
        return presented_view_controller ==
               base_view_controller_.presentedViewController;
      }));

  // Attempt to present QLPreviewController.
  base::FilePath path = GetTestFilePath();
  NSURL* fileURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];
  [coordinator_ ARQuickLookTabHelper:nil
      didFinishDowloadingFileWithURL:fileURL];

  // The attempt is ignored.
  EXPECT_EQ(presented_view_controller,
            base_view_controller_.presentedViewController);

  histogram_tester_.ExpectUniqueSample(
      kIOSPresentQLPreviewControllerHistogram,
      static_cast<base::HistogramBase::Sample>(
          PresentQLPreviewController::kAnotherViewControllerIsPresented),
      1);
}
