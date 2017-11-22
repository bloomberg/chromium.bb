// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_positioner.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_ui_updater.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_consumer.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@class ToolbarButtonFactory;
@class ToolbarConfiguration;

// View controller for a toolbar, which will show a horizontal row of
// controls and/or labels.
// This view controller will fill its container; it is up to the containing
// view controller or presentation controller to configure an appropriate
// height for it.
@interface ToolbarViewController : UIViewController<TabHistoryPositioner,
                                                    TabHistoryUIUpdater,
                                                    ToolbarConsumer>

- (instancetype)initWithDispatcher:
                    (id<ApplicationCommands, BrowserCommands>)dispatcher
                     buttonFactory:(ToolbarButtonFactory*)buttonFactory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// The dispatcher for this view controller.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;

@property(nonatomic, strong) UIView* locationBarView;

// Animates the toolbar so the omnibox is shrinking to its standard state.
- (void)contractOmnibox;
// Expands the omnibox to its expanded state, |animated| or not.
- (void)expandOmniboxAnimated:(BOOL)animated;
// Updates the view so a snapshot can be taken. It needs to be adapted,
// depending on if it is a snapshot displayed |onNTP| or not.
- (void)updateForSideSwipeSnapshotOnNTP:(BOOL)onNTP;
// Resets the view after taking a snapshot for a side swipe.
- (void)resetAfterSideSwipeSnapshot;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_VIEW_CONTROLLER_H_
