// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_LOADING_DELEGATE_H_
#define IOS_CHROME_BROWSER_TABS_TAB_LOADING_DELEGATE_H_

#import <UIKit/UIKit.h>

// A protocol implemented by a delegate managing Tab loading. This protocol
// is used by a Tab that's being preloaded before being displayed to inform the
// entity that's preloading it that it should take some action. In the
// QuickBack system, for example, a QuickBackLoadController will start loading
// a Tab for a search link, and hold it in this loading state until it's
// ready to display. The QuickBackLoadController is then set as this Tab's
// TabLoadingDelegate, which allows the Tab to let its owning controller know
// if it needs to be cancelled or displayed.
@protocol TabLoadingDelegate
// Display the preloading tab immediately.
- (void)displayPreloadingLinkImmediately;
// Cancel preloading tab.
- (void)cancelPreloadingTab;
// Informs the delegate that the tab aborted load internally (e.g., due to
// launching an external URL), so the preloading should be abandoned and the
// source tab restored to its pre-load-attempt state.
- (void)preloadingTabAborted;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_LOADING_DELEGATE_H_
