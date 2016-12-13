// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/crw_fake_web_controller_observer.h"

@implementation CRWFakeWebControllerObserver

@synthesize pageLoaded = _pageLoaded;

- (void)pageLoaded:(CRWWebController*)webController {
  _pageLoaded = YES;
}

@end
