// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_OWNER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_OWNER_H_

#import <Foundation/Foundation.h>

@class ToolbarController;

@protocol ToolbarOwner<NSObject>

// Returns a reference to the toolbar controller so that it can be included in
// animations. Calls should be paired with calls to |-reparentToolbarController|
// when the relinquished toolbar controller is no longer needed by the caller.
// Returns nil if called when the toolbar has already been relinquished.
- (ToolbarController*)relinquishedToolbarController;
// Reparents the toolbar into its place in the view hierarchy before it was
// relinquished.
- (void)reparentToolbarController;

@optional
// Returns the height of the toolbar owned by the implementing class.
- (CGFloat)toolbarHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_OWNER_H_
