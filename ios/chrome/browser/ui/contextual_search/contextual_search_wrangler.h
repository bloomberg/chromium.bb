// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_WRANGLER_H_
#define IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_WRANGLER_H_

#import <UIKit/UIKit.h>

namespace ios {
class ChromeBrowserState;
}
@class Tab;
@class TabModel;

// A provider protocol for Contextual Search; this defines all of the things
// that the Contextual Search system needs to be able to ask for from or change
// in the application as a whole.
@protocol ContextualSearchProvider<NSObject>

// The parent view that the contextual search panel will be a subview of.
@property(nonatomic, readonly) UIView* view;

// The view (if any) containing the tab strip. This may be used for height
// calculations for positioning or aninmating the contextual search view.
@property(nonatomic, readonly) UIView* tabStripView;

// The view containing the toolbar. This should be a subview of the view
// exposed in |view|, above. The contextual search panel will be
// positioned in front of this view, and the contexual search mask will be
// positioned behind it (in the z-axis).
@property(nonatomic, readonly) UIView* toolbarView;

// Tells the provider to set the toolbar control alpha.
- (void)updateToolbarControlsAlpha:(CGFloat)alpha;

// Tells the provider to set the toolbar background alpha.
- (void)updateToolbarBackgroundAlpha:(CGFloat)alpha;

// Tell the provider that a tab load inside contextual search has completed.
// |tab| is that tab that has loaded, and |success| is YES if the tab loaded
// without error, and NO otherwise.
- (void)tabLoadComplete:(Tab*)tab withSuccess:(BOOL)success;

// Tells the pr=ovider to focus the omnibox.
- (void)focusOmnibox;

// Tells the provider to restore any suspended infobars.
- (void)restoreInfobars;

// Tells the provider to suspend any infobars..
- (void)suspendInfobars;

// Asks the provider for the current header offset for positinioning the panel.
- (CGFloat)currentHeaderOffset;

@end

// An object that manages the interactions of the contextual search system with
// the rest of the application. An instance of this object handles creating and
// positioning all of the views required for contextual search, as well as all
// of the other controller objects.
@interface ContextualSearchWrangler : NSObject

// Creates a new wrangler object for |provider| and |tabModel|.
- (instancetype)initWithProvider:(id<ContextualSearchProvider>)provider
                        tabModel:(TabModel*)tabModel NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Inserts the panel view into the view hierarchy provided by the provider the
// receiver was initialized with.
- (void)insertPanelView;

// Starts contextual search using the receiver for |browserState|. This should
// only be called once in the lifetime of the receiver. If it is possible
// (as determined by TouchToSearchPermissionsMediator) to start contextual
// search on this device, and |browserState| isn't incognito, then this method
// will create the contextual search controller objects and views and start
// observing the tab model it was initialized with.
- (void)maybeStartForBrowserState:(ios::ChromeBrowserState*)browserState;

// Enables or disables contextual search for the receiver; this may be called
// multiple times during the lifetime of the receiver.
- (void)enable:(BOOL)enabled;

// Tears down the controller and views the receiver has created. This method
// must be called before the receiver is deallocated.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_WRANGLER_H_
