// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_EXTERNAL_APP_LAUNCHER_H_
#define IOS_CHROME_BROWSER_WEB_EXTERNAL_APP_LAUNCHER_H_

#import <UIKit/UIKit.h>

class GURL;

// A customized external app launcher that optionally shows a modal
// confirmation dialog before switching context to an external application.
@interface ExternalAppLauncher : NSObject

// Opens URL in an external application if possible (optionally after
// confirming via dialog in case that user didn't interact using
// |linkClicked| or if the external application is face time) or returns NO
// if there is no such application available.
- (BOOL)openURL:(const GURL&)gURL linkClicked:(BOOL)linkClicked;

@end

#endif  // IOS_CHROME_BROWSER_WEB_EXTERNAL_APP_LAUNCHER_H_
