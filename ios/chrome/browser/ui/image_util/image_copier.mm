// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "image_copier.h"

#include "base/task/post_task.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/web_thread.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Time Period between "Copy Image" is clicked and "Copying..." alert is
// launched.
const int kAlertDelayInMs = 300;
// A unique id indicates that last session is finished or canceled and next
// session has not started.
const ImageCopierSessionID ImageCopierNoSession = -1;
}

@interface ImageCopier ()
// Base view controller for the alerts.
@property(nonatomic, weak) UIViewController* baseViewController;
// Alert coordinator to give feedback to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
// ID of current copying session.
@property(nonatomic) ImageCopierSessionID currentSessionID;
@end

@implementation ImageCopier

@synthesize alertCoordinator = _alertCoordinator;
@synthesize baseViewController = _baseViewController;
@synthesize currentSessionID = _currentSessionID;

- (instancetype)initWithBaseViewController:
    (UIViewController*)baseViewController {
  self = [super init];
  if (self) {
    self.baseViewController = baseViewController;
  }
  return self;
}

// Use current timestamp as identifier for the session. The "Copying..." alert
// will be launched after |kAlertDelayInMs| so that user won't notice a fast
// download.
- (ImageCopierSessionID)beginSession {
  // Dismiss current alert.
  [self.alertCoordinator stop];

  __weak ImageCopier* weakSelf = self;

  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                           title:l10n_util::GetNSStringWithFixup(
                                     IDS_IOS_CONTENT_COPYIMAGE_ALERT_COPYING)
                         message:nil];
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)
                action:^() {
                  // Cancel current session and close the alert.
                  weakSelf.currentSessionID = ImageCopierNoSession;
                  [weakSelf.alertCoordinator stop];
                }
                 style:UIAlertActionStyleCancel];

  double time = [NSDate timeIntervalSinceReferenceDate];
  ImageCopierSessionID sessionID = *(int64_t*)(&time);
  self.currentSessionID = sessionID;

  // Delay launching alert by |IMAGE_COPIER_ALERT_DELAY_MS|.
  web::WebThread::PostDelayedTask(
      web::WebThread::UI, FROM_HERE, base::BindOnce(^{
        // Check that the session has not finished yet.
        if (sessionID == weakSelf.currentSessionID) {
          [weakSelf.alertCoordinator start];
        }
      }),
      base::TimeDelta::FromMilliseconds(kAlertDelayInMs));

  return sessionID;
}

// End the session. If the session is not canceled, paste the image data to
// system's pasteboard.
- (void)endSession:(ImageCopierSessionID)sessionID withImageData:(NSData*)data {
  // Check that the downloading session is not canceled.
  if (sessionID == self.currentSessionID) {
    if (data && data.length > 0) {
      [UIPasteboard.generalPasteboard setImage:[UIImage imageWithData:data]];
    }
    self.currentSessionID = ImageCopierNoSession;
    [self.alertCoordinator stop];
  }
}

@end
