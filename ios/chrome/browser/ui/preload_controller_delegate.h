// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRELOAD_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_PRELOAD_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

// A protocol implemented by a delegate of PreloadController
@protocol PreloadControllerDelegate

// Should preload controller request a desktop site.
- (BOOL)preloadShouldUseDesktopUserAgent;

// Returns YES if the given |url| should be backed by a native controller.
- (BOOL)preloadHasNativeControllerForURL:(const GURL&)url;
@end

#endif  // IOS_CHROME_BROWSER_UI_PRELOAD_CONTROLLER_DELEGATE_H_
