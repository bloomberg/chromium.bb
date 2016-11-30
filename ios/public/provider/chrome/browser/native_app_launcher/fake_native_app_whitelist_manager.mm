// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/native_app_launcher/fake_native_app_whitelist_manager.h"

#include "base/mac/scoped_nsobject.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeNativeAppWhitelistManager

@synthesize metadata = _metadata;
@synthesize appWhitelist = _appWhitelist;
@synthesize appScheme = _appScheme;

- (void)setAppList:(NSArray*)appList
           tldList:(NSArray*)tldList
    acceptStoreIDs:(NSArray*)storeIDs {
  _appWhitelist = appList;
}

- (id<NativeAppMetadata>)newNativeAppForURL:(const GURL&)url {
  return _metadata;
}

- (NSArray*)filteredAppsUsingBlock:(NativeAppFilter)condition {
  return _appWhitelist;
}

- (NSURL*)schemeForAppId:(NSString*)appId {
  return [NSURL URLWithString:[NSString stringWithFormat:@"%@://", _appScheme]];
}

- (void)checkInstalledApps {
  return;
}

@end
