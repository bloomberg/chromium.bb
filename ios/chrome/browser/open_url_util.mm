// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/open_url_util.h"

#import <UIKit/UIKit.h>

#include "base/ios/ios_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void OpenUrlWithCompletionHandler(NSURL* url,
                                  void (^completion_handler)(BOOL success)) {
  if (base::ios::IsRunningOnIOS10OrLater()) {
    [[UIApplication sharedApplication] openURL:url
                                       options:@{}
                             completionHandler:completion_handler];
  } else {
    BOOL result = [[UIApplication sharedApplication] openURL:url];
    if (!completion_handler)
      return;
    dispatch_async(dispatch_get_main_queue(), ^{
      completion_handler(result);
    });
  }
}
