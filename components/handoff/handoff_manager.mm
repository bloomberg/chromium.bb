// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/handoff/handoff_manager.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "components/handoff/handoff_utility.h"
#include "net/base/mac/url_conversions.h"

#if defined(OS_IOS)
#include "base/ios/ios_util.h"
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#endif

@interface HandoffManager ()

// The active user activity.
@property(nonatomic, retain) NSUserActivity* userActivity;

// Whether the URL of the current tab should be exposed for Handoff.
- (BOOL)shouldUseActiveURL;

// Updates the active NSUserActivity.
- (void)updateUserActivity;

@end

@implementation HandoffManager

@synthesize userActivity = _userActivity;

- (instancetype)init {
  self = [super init];
  if (self) {
    _propertyReleaser_HandoffManager.Init(self, [HandoffManager class]);
  }
  return self;
}

- (void)updateActiveURL:(const GURL&)url {
#if defined(OS_IOS)
  // Handoff is only available on iOS 8+.
  DCHECK(base::ios::IsRunningOnIOS8OrLater());
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Handoff is only available on OSX 10.10+.
  DCHECK(base::mac::IsOSYosemiteOrLater());
#endif

  _activeURL = url;
  [self updateUserActivity];
}

- (BOOL)shouldUseActiveURL {
  return _activeURL.SchemeIsHTTPOrHTTPS();
}

- (void)updateUserActivity {
  // Clear the user activity.
  if (![self shouldUseActiveURL]) {
    [self.userActivity invalidate];
    self.userActivity = nil;
    return;
  }

  // No change to the user activity.
  const GURL userActivityURL(net::GURLWithNSURL(self.userActivity.webpageURL));
  if (userActivityURL == _activeURL)
    return;

  // Invalidate the old user activity and make a new one.
  [self.userActivity invalidate];

  Class aClass = NSClassFromString(@"NSUserActivity");
  NSUserActivity* userActivity = [[aClass performSelector:@selector(alloc)]
      performSelector:@selector(initWithActivityType:)
           withObject:handoff::kChromeHandoffActivityType];
  self.userActivity = base::scoped_nsobject<NSUserActivity>(userActivity);
  self.userActivity.webpageURL = net::NSURLWithGURL(_activeURL);
  self.userActivity.userInfo = @{ handoff::kOriginKey : handoff::kOriginiOS };
  [self.userActivity becomeCurrent];
}

@end
