// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_PANEL_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_PANEL_VIEW_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/contextual_search/panel_configuration.h"

@protocol ContextualSearchPanelMotionObserver;
@protocol ContextualSearchPanelTapHandler;

// A view designed to sit "on top" of the frontmost tab in a range of positions,
// with content can position controlled by a ContextualSearchPanelController.
// Generally speaking each BrowserViewController will own both the panel view
// and panel controller object.
@interface ContextualSearchPanelView : UIView

// Current state.
@property(nonatomic, assign) ContextualSearch::PanelState state;
// Panel configuration, for motion observers that want to do different
// computations around panel state and position.
@property(nonatomic, readonly) PanelConfiguration* configuration;

// Create a panel view. It will need to have a delegate and controller assigned
// to do anything useful.
- (instancetype)initWithConfiguration:(PanelConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

// Add panel content views. Views in |contentViews| will be arranged vertically
// in the panel according to their intrinsic sizes, and will fill its width.
// If a view in |contentViews| conforms to the panel motion observer protocol,
// it will be automatically added as an observer.
// If a view in |contentViews| conforms to the panel scroll synchronizer
// protocol, its scrolling will be synchronized with panel dragging.
- (void)addContentViews:(NSArray*)contentViews;

// Add or remove observers.
- (void)addMotionObserver:(id<ContextualSearchPanelMotionObserver>)observer;
- (void)removeMotionObserver:(id<ContextualSearchPanelMotionObserver>)observer;

// Inform the receiver it is about to promote to be tab-sized; it will inform
// any obsevers.
- (void)prepareForPromotion;
// Have the receiver adjust its frame to match its superview's bounds,
// vertically offset by |offset| points from the y-origin.
- (void)promoteToMatchSuperviewWithVerticalOffset:(CGFloat)offset;
@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_PANEL_VIEW_H_
