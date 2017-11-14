// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_positioner.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_presentation.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_ui_updater.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_consumer.h"
#import "ios/clean/chrome/browser/ui/transitions/animators/zoom_transition_delegate.h"

@protocol NavigationCommands;
@protocol TabGridCommands;
@protocol TabHistoryPopupCommands;
@protocol TabStripCommands;
@class CleanToolbarButtonFactory;
@protocol ToolsMenuCommands;

// View controller for a toolbar, which will show a horizontal row of
// controls and/or labels.
// This view controller will fill its container; it is up to the containing
// view controller or presentation controller to configure an appropriate
// height for it.
@interface CleanToolbarViewController : UIViewController<TabHistoryPositioner,
                                                         TabHistoryPresentation,
                                                         TabHistoryUIUpdater,
                                                         CleanToolbarConsumer,
                                                         ZoomTransitionDelegate>

- (instancetype)initWithDispatcher:(id<NavigationCommands,
                                       TabGridCommands,
                                       TabHistoryPopupCommands,
                                       TabStripCommands,
                                       ToolsMenuCommands>)dispatcher
                     buttonFactory:(CleanToolbarButtonFactory*)buttonFactory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// The dispatcher for this view controller.
@property(nonatomic, weak) id<NavigationCommands,
                              TabGridCommands,
                              TabHistoryPopupCommands,
                              TabStripCommands,
                              ToolsMenuCommands>
    dispatcher;

@property(nonatomic, strong) UIViewController* locationBarViewController;

// By default, this view controller does not interact with the tab strip. When
// |usesTabStrip| is YES, the tab switcher button first displays the tab strip.
// A second tap on the tab switcher displays the tab grid.
@property(nonatomic, assign) BOOL usesTabStrip;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_VIEW_CONTROLLER_H_
