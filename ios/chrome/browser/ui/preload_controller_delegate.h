// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRELOAD_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_PRELOAD_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@class CRWSessionEntry;

// A protocol implemented by a delegate of PreloadController
@protocol PreloadControllerDelegate

// Should preload controller request a desktop site.
- (BOOL)shouldUseDesktopUserAgent;
// Return the current sessionEntry from the delegate.
// TODO(crbug.com/546348): See if this can return a NavigationItem instead.
- (CRWSessionEntry*)currentSessionEntry;
@end

#endif  // IOS_CHROME_BROWSER_UI_PRELOAD_CONTROLLER_DELEGATE_H_
