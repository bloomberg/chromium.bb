// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/native_app_launcher/fake_native_app_metadata.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeNativeAppMetadata

@synthesize shouldBypassInfoBars = _shouldBypassInfoBars;
@synthesize shouldAutoOpenLinks = _shouldAutoOpenLinks;
@synthesize numberOfDismissedInfoBars = _numberOfDismissedInfoBars;
@synthesize googleOwnedApp = _googleOwnedApp;
@synthesize installed = _installed;
@synthesize appName = _appName;
@synthesize appId = _appId;

- (void)unsetShouldAutoOpenLinks {
}

- (NSString*)appStoreURL {
  return nil;
}

- (NSURL*)appURLforURL:(NSURL*)url {
  return nil;
}

- (void)fetchSmallIconWithImageFetcher:
            (image_fetcher::IOSImageDataFetcherWrapper*)imageFetcher
                       completionBlock:(void (^)(UIImage*))block {
  block(nil);
}

- (BOOL)canOpenURL:(const GURL&)url {
  if (!url.is_valid())
    return YES;
  GURL appUrl(base::SysNSStringToUTF8(_appName) + ":");
  if (url.spec() == appUrl.spec()) {
    return YES;
  } else {
    return NO;
  }
}

- (GURL)launchURLWithURL:(const GURL&)gurl identity:(ChromeIdentity*)identity {
  return GURL();
}

- (void)resetInfobarHistory {
}

- (void)enumerateSchemesWithBlock:(void (^)(NSString* scheme,
                                            BOOL* stop))block {
}

- (void)updateCounterWithAppInstallation {
}

- (NSString*)anyScheme {
  return nil;
}

- (void)willBeShownInInfobarOfType:(NativeAppControllerType)type {
}

- (void)updateWithUserAction:(NativeAppActionType)userAction {
}

@end
