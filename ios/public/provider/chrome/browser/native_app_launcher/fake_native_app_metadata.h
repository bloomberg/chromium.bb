// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_NATIVE_APP_LAUNCHER_FAKE_NATIVE_APP_METADATA_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_NATIVE_APP_LAUNCHER_FAKE_NATIVE_APP_METADATA_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_metadata.h"

// FakeNativeAppMetadata used for testing purposes.
@interface FakeNativeAppMetadata : NSObject<NativeAppMetadata>

// Name of the FakeNativeAppMetadata App.
@property(nonatomic, copy, readwrite) NSString* appName;

// Id of the FakeNativeAppMetadata App.
@property(nonatomic, copy, readwrite) NSString* appId;

// Checks if the FakeNativeApp is Google owned.
@property(nonatomic, readwrite, getter=isGoogleOwnedApp) BOOL googleOwnedApp;

// Checks if the FakeNativeApp is installed.
@property(nonatomic, readwrite, getter=isInstalled) BOOL installed;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_NATIVE_APP_LAUNCHER_FAKE_NATIVE_APP_METADATA_H_
