// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_ABSTRACT_WEB_TOOLBAR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_ABSTRACT_WEB_TOOLBAR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/toolbar/public/abstract_toolbar.h"

@class Tab;
@protocol WebToolbarDelegate;

// WebToolbarController public interface.
@protocol AbstractWebToolbar<AbstractToolbar>
// Called when the browser state this object was initialized with is being
// destroyed.
- (void)browserStateDestroyed;
// Update the visibility of the back/forward buttons, omnibox, etc.
- (void)updateToolbarState;
// Briefly animate the progress bar when a pre-rendered tab is displayed.
- (void)showPrerenderingAnimation;
// Called when the current page starts loading.
- (void)currentPageLoadStarted;
// Returns visible omnibox frame in WebToolbarController's view coordinate
// system.
- (CGRect)visibleOmniboxFrame;
// Returns whether omnibox is a first responder.
- (BOOL)isOmniboxFirstResponder;
// Returns whether the omnibox popup is currently displayed.
- (BOOL)showingOmniboxPopup;
// Called when the current tab changes or is closed.
- (void)selectedTabChanged;
// Update the visibility of the toolbar before making a side swipe snapshot so
// the toolbar looks appropriate for |tab|. This includes morphing the toolbar
// to look like the new tab page header.
- (void)updateToolbarForSideSwipeSnapshot:(Tab*)tab;
// Remove any formatting added by -updateToolbarForSideSwipeSnapshot.
- (void)resetToolbarAfterSideSwipeSnapshot;
// WebToolbarDelegate delegate.
@property(nonatomic, weak) id<WebToolbarDelegate> delegate;
@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_ABSTRACT_WEB_TOOLBAR_H_
