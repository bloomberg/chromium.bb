// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/startup/background_upload_alert.h"

#include "ios/chrome/browser/experimental_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BackgroundUploadAlert

+ (void)setupBackgroundUploadAlert {
  if (experimental_flags::IsAlertOnBackgroundUploadEnabled()) {
    if ([UIApplication instancesRespondToSelector:
                           @selector(registerUserNotificationSettings:)]) {
      [[UIApplication sharedApplication]
          registerUserNotificationSettings:
              [UIUserNotificationSettings
                  settingsForTypes:UIUserNotificationTypeAlert |
                                   UIUserNotificationTypeBadge |
                                   UIUserNotificationTypeSound
                        categories:nil]];
    }
  }
}

@end
