// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TABS_TAB_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TABS_TAB_CONTROLLER_H_

#import <Cocoa/Cocoa.h>
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_drag_controller.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "url/gurl.h"

// The loading/waiting state of the tab.
enum TabLoadingState {
  kTabDone,
  kTabLoading,
  kTabWaiting,
  kTabCrashed,
};

@class MediaIndicatorView;
@class MenuController;
namespace TabControllerInternal {
class MenuDelegate;
}
@class SpriteView;
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
  base::scoped_nsobject<SpriteView> iconView_;
  base::scoped_nsobject<MediaIndicatorView> mediaIndicatorView_;
  base::scoped_nsobject<HoverCloseButton> closeButton_;

  NSRect originalIconFrame_;  // frame of iconView_ as loaded from nib
  BOOL isIconShowing_;  // last state of iconView_ in updateVisibility

  BOOL app_;
  BOOL mini_;
  BOOL pinned_;
  BOOL active_;
  BOOL selected_;
  GURL url_;
  TabLoadingState loadingState_;
  id<TabControllerTarget> target_;  // weak, where actions are sent
  SEL action_;  // selector sent when tab is selected by clicking
  scoped_ptr<ui::SimpleMenuModel> contextMenuModel_;
  scoped_ptr<TabControllerInternal::MenuDelegate> contextMenuDelegate_;
  base::scoped_nsobject<MenuController> contextMenuController_;
}

@property(assign, nonatomic) TabLoadingState loadingState;

@property(assign, nonatomic) SEL action;
@property(assign, nonatomic) BOOL app;
@property(assign, nonatomic) BOOL mini;
@property(assign, nonatomic) BOOL pinned;
@property(assign, nonatomic) NSString* toolTip;
// Note that |-selected| will return YES if the controller is |-active|, too.
// |-setSelected:| affects the selection, while |-setActive:| affects the key
// status/focus of the content.
@property(assign, nonatomic) BOOL active;
@property(assign, nonatomic) BOOL selected;
@property(assign, nonatomic) id target;
@property(assign, nonatomic) GURL url;
@property(readonly, nonatomic) NSView* iconView;
@property(assign, nonatomic) MediaIndicatorView* mediaIndicatorView;
@property(readonly, nonatomic) HoverCloseButton* closeButton;

// Minimum and maximum allowable tab width. The minimum width does not show
// the icon or the close button. The active tab always has at least a close
// button so it has a different minimum width.
+ (CGFloat)minTabWidth;
+ (CGFloat)maxTabWidth;
+ (CGFloat)minActiveTabWidth;
+ (CGFloat)miniTabWidth;
+ (CGFloat)appTabWidth;

// The view associated with this controller, pre-casted as a TabView
- (TabView*)tabView;

// Sets the tab's icon image.
// |image| must be 16x16 in size.
// |image| can be a horizontal strip of image sprites which will be animated.
// Setting |animate| to YES will animate away the old image before animating
// the new image back to position.
- (void)setIconImage:(NSImage*)image;
- (void)setIconImage:(NSImage*)image withToastAnimation:(BOOL)animate;

// Closes the associated TabView by relaying the message to |target_| to
// perform the close.
- (void)closeTab:(id)sender;

// Selects the associated TabView by sending |action_| to |target_|.
- (void)selectTab:(id)sender;

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
- (int)iconCapacity;
- (BOOL)shouldShowIcon;
- (BOOL)shouldShowMediaIndicator;
- (BOOL)shouldShowCloseButton;
@end  // TabController(TestingAPI)

#endif  // CHROME_BROWSER_UI_COCOA_TABS_TAB_CONTROLLER_H_
