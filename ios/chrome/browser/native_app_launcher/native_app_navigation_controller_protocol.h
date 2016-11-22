// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_NAVIGATION_CONTROLLER_PROTOCOL_H_
#define IOS_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_NAVIGATION_CONTROLLER_PROTOCOL_H_

#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_types.h"

class GURL;
@class NSString;
@class UIImage;

// Protocol required for a controller to create a Native App Launcher infobar.
@protocol NativeAppNavigationControllerProtocol
// Returns app ID used to offer the installation of the application
- (NSString*)appId;
// Returns app name displayed in the infobar.
- (NSString*)appName;
// Asynchronously fetches icon and calls |block| when done.
- (void)fetchSmallIconWithCompletionBlock:(void (^)(UIImage*))block;
// Launches the application from the information extracted from |url|.
- (void)launchApp:(const GURL&)url;
// Opens store for the installation of the application.
- (void)openStore;
// Update metadata based on what the user did with the infobar.
- (void)updateMetadataWithUserAction:(NativeAppActionType)userAction;
@end

#endif  // IOS_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_NAVIGATION_CONTROLLER_PROTOCOL_H_
