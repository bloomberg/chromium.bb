// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/ar_quick_look_coordinator.h"

#import <QuickLook/QuickLook.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kIOSPresentQLPreviewControllerHistogram[] =
    "Download.IOSPresentQLPreviewControllerResult";

namespace {

// Returns an enum for Download.IOSPresentQLPreviewControllerResult.
PresentQLPreviewController GetHistogramEnum(
    UIViewController* base_view_controller,
    bool is_file_valid) {
  if (!is_file_valid) {
    return PresentQLPreviewController::kInvalidFile;
  }

  if (!base_view_controller.presentedViewController) {
    return PresentQLPreviewController::kSuccessful;
  }

  if ([base_view_controller.presentedViewController
          isKindOfClass:[QLPreviewController class]]) {
    return PresentQLPreviewController::kAnotherQLPreviewControllerIsPresented;
  }

  return PresentQLPreviewController::kAnotherViewControllerIsPresented;
}

}  // namespace

@interface ARQuickLookCoordinator () <QLPreviewControllerDataSource,
                                      QLPreviewControllerDelegate>

// The file URL pointing to the downloaded USDZ format file.
@property(nonatomic, copy) NSURL* fileURL;

// Displays USDZ format files.
@property(nonatomic, strong) QLPreviewController* viewController;

@end

@implementation ARQuickLookCoordinator

- (void)stop {
  [self.viewController dismissViewControllerAnimated:YES completion:nil];
  self.viewController = nil;
  self.fileURL = nil;
}

- (void)start {
  UMA_HISTOGRAM_ENUMERATION(
      kIOSPresentQLPreviewControllerHistogram,
      GetHistogramEnum(self.baseViewController, !!self.fileURL));

  if (self.viewController || !self.fileURL) {
    return;
  }

  self.viewController = [[QLPreviewController alloc] init];
  self.viewController.dataSource = self;
  self.viewController.delegate = self;
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - ARQuickLookTabHelperDelegate

- (void)ARQuickLookTabHelper:(ARQuickLookTabHelper*)tabHelper
    didFinishDowloadingFileWithURL:(NSURL*)fileURL {
  self.fileURL = fileURL;
  [self start];
}

#pragma mark - QLPreviewControllerDataSource

- (NSInteger)numberOfPreviewItemsInPreviewController:
    (QLPreviewController*)controller {
  return 1;
}

- (id<QLPreviewItem>)previewController:(QLPreviewController*)controller
                    previewItemAtIndex:(NSInteger)index {
  return self.fileURL;
}

#pragma mark - QLPreviewControllerDelegate

- (void)previewControllerDidDismiss:(QLPreviewController*)controller {
  [self stop];
}

@end
