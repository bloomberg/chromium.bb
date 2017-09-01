// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/transitions/animators/zoom_transition_delegate.h"
#import "ios/clean/chrome/browser/ui/transitions/presenters/menu_presentation_delegate.h"

@protocol ContainmentTransitioningDelegate;

// Abstract base class for a view controller that contains several views,
// each managed by their own view controllers.
// Subclasses manage the specific layout of these view controllers.
@interface TabContainerViewController
    : UIViewController<MenuPresentationDelegate, ZoomTransitionDelegate>

// View controller showing the main content for the tab. If there is no
// toolbar view controller set, the contents of this view controller will
// fill all of the tab container's view.
@property(nonatomic, strong) UIViewController* contentViewController;

// View controller showing the toolbar for the tab. It will be of a fixed
// height (determined internally by the tab container), but will span the
// width of the tab.
@property(nonatomic, strong) UIViewController* toolbarViewController;

// View controller showing the tab strip. It will be of a fixed
// height (determined internally by the tab container), but will span the
// width of the tab.
@property(nonatomic, strong) UIViewController* tabStripViewController;

// YES if |tabStripViewController| is currently visible.
@property(nonatomic, getter=isTabStripVisible) BOOL tabStripVisible;

// View controller showing the find bar.  The location of this controller's view
// is determined by size class and device type.  May be nil if the find bar is
// currently closed.
@property(nonatomic, strong) UIViewController* findBarViewController;

// Transitioning delegate for containment animations. By default it's the
// tab container view controller itself.
@property(nonatomic, weak) id<ContainmentTransitioningDelegate>
    containmentTransitioningDelegate;

@end

// Tab container which positions the toolbar at the top.
@interface TopToolbarTabViewController : TabContainerViewController
@end

// Tab container which positions the toolbar at the bottom.
@interface BottomToolbarTabViewController : TabContainerViewController
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_CONTAINER_VIEW_CONTROLLER_H_
