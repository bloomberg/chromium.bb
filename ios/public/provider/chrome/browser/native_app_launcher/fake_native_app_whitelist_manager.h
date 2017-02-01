// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_NATIVE_APP_LAUNCHER_FAKE_NATIVE_APP_WHITELIST_MANAGER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_NATIVE_APP_LAUNCHER_FAKE_NATIVE_APP_WHITELIST_MANAGER_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_whitelist_manager.h"

// Fake NativeAppWhitelistManager used for testing purposes.
@interface FakeNativeAppWhitelistManager : NSObject<NativeAppWhitelistManager>

// The metadata returned by calls to |nativeAppForURL:|.
@property(nonatomic, strong, readwrite) id<NativeAppMetadata> metadata;

// The Apps array returned by calls to |filteredAppsUsingBlock:|.
@property(nonatomic, strong, readwrite) NSArray* appWhitelist;

// The String that will be used to create the NSURL for |schemeForAppId:|.
@property(nonatomic, copy, readwrite) NSString* appScheme;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_NATIVE_APP_LAUNCHER_FAKE_NATIVE_APP_WHITELIST_MANAGER_H_
