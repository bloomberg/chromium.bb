// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_ABSTRACT_WEB_TOOLBAR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_ABSTRACT_WEB_TOOLBAR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/toolbar/public/abstract_toolbar.h"

@class Tab;
@class ToolbarButtonUpdater;

// WebToolbarController public interface.
@protocol AbstractWebToolbar<AbstractToolbar>
// Briefly animate the progress bar when a pre-rendered tab is displayed.
- (void)showPrerenderingAnimation;
// Returns whether omnibox is a first responder.
- (BOOL)isOmniboxFirstResponder;
// Returns whether the omnibox popup is currently displayed.
- (BOOL)showingOmniboxPopup;
// Update the visibility of the toolbar before making a side swipe snapshot so
// the toolbar looks appropriate for |tab|. This includes morphing the toolbar
// to look like the new tab page header.
- (void)updateToolbarForSideSwipeSnapshot:(Tab*)tab;
// Remove any formatting added by -updateToolbarForSideSwipeSnapshot.
- (void)resetToolbarAfterSideSwipeSnapshot;
// Convenience getter for the UIViewController.
@property(nonatomic, readonly, weak) UIViewController* viewController;
// Object handling the updates of the buttons.
@property(nonatomic, strong, readonly) ToolbarButtonUpdater* buttonUpdater;
@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_ABSTRACT_WEB_TOOLBAR_H_
