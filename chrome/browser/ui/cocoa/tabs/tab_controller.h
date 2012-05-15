// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TABS_TAB_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TABS_TAB_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_drag_controller.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "googleurl/src/gurl.h"

// The loading/waiting state of the tab.
enum TabLoadingState {
  kTabDone,
  kTabLoading,
  kTabWaiting,
  kTabCrashed,
};

@class MenuController;
namespace TabControllerInternal {
class MenuDelegate;
}
@class TabView;
@protocol TabControllerTarget;

// A class that manages a single tab in the tab strip. Set its target/action
// to be sent a message when the tab is selected by the user clicking. Setting
// the |loading| property to YES visually indicates that this tab is currently
// loading content via a spinner.
//
// The tab has the notion of an "icon view" which can be used to display
// identifying characteristics such as a favicon, or since it's a full-fledged
// view, something with state and animation such as a throbber for illustrating
// progress. The default in the nib is an image view so nothing special is
// required if that's all you need.

@interface TabController : NSViewController<TabDraggingEventTarget> {
 @private
  IBOutlet NSView* iconView_;
  IBOutlet NSTextField* titleView_;
  IBOutlet HoverCloseButton* closeButton_;

  NSRect originalIconFrame_;  // frame of iconView_ as loaded from nib
  BOOL isIconShowing_;  // last state of iconView_ in updateVisibility

  BOOL app_;
  BOOL mini_;
  BOOL pinned_;
  BOOL active_;
  BOOL selected_;
  GURL url_;
  TabLoadingState loadingState_;
  CGFloat iconTitleXOffset_;  // between left edges of icon and title
  id<TabControllerTarget> target_;  // weak, where actions are sent
  SEL action_;  // selector sent when tab is selected by clicking
  scoped_ptr<ui::SimpleMenuModel> contextMenuModel_;
  scoped_ptr<TabControllerInternal::MenuDelegate> contextMenuDelegate_;
  scoped_nsobject<MenuController> contextMenuController_;
}

@property(assign, nonatomic) TabLoadingState loadingState;

@property(assign, nonatomic) SEL action;
@property(assign, nonatomic) BOOL app;
@property(assign, nonatomic) BOOL mini;
@property(assign, nonatomic) BOOL pinned;
// Note that |-selected| will return YES if the controller is |-active|, too.
// |-setSelected:| affects the selection, while |-setActive:| affects the key
// status/focus of the content.
@property(assign, nonatomic) BOOL active;
@property(assign, nonatomic) BOOL selected;
@property(assign, nonatomic) id target;
@property(assign, nonatomic) GURL url;
@property(assign, nonatomic) NSView* iconView;
@property(assign, nonatomic) NSTextField* titleView;
@property(assign, nonatomic) HoverCloseButton* closeButton;

// Minimum and maximum allowable tab width. The minimum width does not show
// the icon or the close button. The selected tab always has at least a close
// button so it has a different minimum width.
+ (CGFloat)minTabWidth;
+ (CGFloat)maxTabWidth;
+ (CGFloat)minSelectedTabWidth;
+ (CGFloat)miniTabWidth;
+ (CGFloat)appTabWidth;

// The view associated with this controller, pre-casted as a TabView
- (TabView*)tabView;

// Closes the associated TabView by relaying the message to |target_| to
// perform the close.
- (IBAction)closeTab:(id)sender;

// Replace the current icon view with the given view. |iconView| will be
// resized to the size of the current icon view.
- (void)setIconView:(NSView*)iconView;
- (NSView*)iconView;

// Called by the tabs to determine whether we are in rapid (tab) closure mode.
// In this mode, we handle clicks slightly differently due to animation.
// Ideally, tabs would know about their own animation and wouldn't need this.
- (BOOL)inRapidClosureMode;

// Updates the visibility of certain subviews, such as the icon and close
// button, based on criteria such as the tab's selected state and its current
// width.
- (void)updateVisibility;

// Update the title color to match the tabs current state.
- (void)updateTitleColor;
@end

@interface TabController(TestingAPI)
- (NSString*)toolTip;
- (int)iconCapacity;
- (BOOL)shouldShowIcon;
- (BOOL)shouldShowCloseButton;
@end  // TabController(TestingAPI)

#endif  // CHROME_BROWSER_UI_COCOA_TABS_TAB_CONTROLLER_H_
