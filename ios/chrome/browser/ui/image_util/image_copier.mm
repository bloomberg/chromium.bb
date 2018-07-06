// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "image_copier.h"

#include "base/task_scheduler/post_task.h"
#import "ios/chrome/browser/ui/alert_coordinator/loading_alert_coordinator.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Time Period between "Copy Image" is clicked and "Copying..." alert is
// launched.
static const int IMAGE_COPIER_ALERT_DELAY_MS = 300;
// A unique id indicates that last session is finished or canceled and next
// session has not started.
static const ImageCopierSessionId ImageCopierNoSession = -1;

@interface ImageCopier ()
// Base view controller for the alerts.
@property(nonatomic, weak) UIViewController* baseViewController;
// Alert coordinator to give feedback to the user.
@property(nonatomic, strong) LoadingAlertCoordinator* alertCoordinator;
@end

@implementation ImageCopier {
  ImageCopierSessionId current_session_id_;
}

@synthesize alertCoordinator = _loadingAlertCoordinator;
@synthesize baseViewController = _baseViewController;

- (instancetype)initWithBaseViewController:
    (UIViewController*)baseViewController {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
  }
  return self;
}

// Use current timestamp as identifier for the session. The "Copying..." alert
// will be launched after |IMAGE_COPIER_ALERT_DELAY_MS| so that user won't
// notice a fast download.
- (ImageCopierSessionId)beginSession {
  // Dismiss current alert.
  [_loadingAlertCoordinator stop];

  self.alertCoordinator = [[LoadingAlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                           title:l10n_util::GetNSStringWithFixup(
                                     IDS_IOS_CONTENT_CONTEXT_COPYING)
                   cancelHandler:^() {
                     // Cancel current session and close the alert.
                     current_session_id_ = ImageCopierNoSession;
                     [_loadingAlertCoordinator stop];
                   }];

  double time = [NSDate timeIntervalSinceReferenceDate];
  ImageCopierSessionId image_id = *(int64_t*)(&time);
  current_session_id_ = image_id;

  // Delay launching alert by |IMAGE_COPIER_ALERT_DELAY_MS|.
  base::PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        // Check that the session has not finished yet.
        if (image_id == current_session_id_) {
          [_loadingAlertCoordinator start];
        }
      }),
      base::TimeDelta::FromMilliseconds(IMAGE_COPIER_ALERT_DELAY_MS));

  return image_id;
}

// End the session. If the session is not canceled, paste the image data to
// system's pasteboard.
- (void)endSession:(ImageCopierSessionId)image_id withImageData:(NSData*)data {
  // Check that the downloading session is not canceled.
  if (image_id == current_session_id_ && data && data.length > 0) {
    UIPasteboard* pasteBoard = [UIPasteboard generalPasteboard];
    [pasteBoard setImage:[UIImage imageWithData:data]];
    [_loadingAlertCoordinator stop];
    current_session_id_ = ImageCopierNoSession;
  }
}

@end
