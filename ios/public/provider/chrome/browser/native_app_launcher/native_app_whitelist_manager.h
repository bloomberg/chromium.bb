// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_WHITELIST_MANAGER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_WHITELIST_MANAGER_H_

#import <Foundation/Foundation.h>

class GURL;
@protocol NativeAppMetadata;
typedef BOOL (^NativeAppFilter)(const id<NativeAppMetadata> app, BOOL* stop);

@protocol NativeAppWhitelistManager<NSObject>

// Sets a new application list to this object.
// The |tldList| is an array of NSStrings enumerating supported top level
// domains without the "." (i.e. @"com" and not @".com").
// |storeIDs| is an optional array of App Store IDs to select only
// listed apps from |appList|. If |storeIDs| is nil, all apps from |appList|
// will be used.
- (void)setAppList:(NSArray*)appList
           tldList:(NSArray*)tldList
    acceptStoreIDs:(NSArray*)storeIDs;

// Returns an object with the metadata about the iOS native application that
// can handle |url|. Returns nil otherwise.
- (id<NativeAppMetadata>)nativeAppForURL:(const GURL&)url;

// Returns an autoreleased NSArray of NativeAppMetadata objects which
// |condition| block returns YES.
- (NSArray*)filteredAppsUsingBlock:(NativeAppFilter)condition;

// Returns a scheme opening the app. Returns nil if no scheme is found.
- (NSURL*)schemeForAppId:(NSString*)appId;

// Checks for installed Apps.
- (void)checkInstalledApps;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_WHITELIST_MANAGER_H_
