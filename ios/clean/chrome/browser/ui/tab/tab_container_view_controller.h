// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/animators/zoom_transition_delegate.h"
#import "ios/clean/chrome/browser/ui/presenters/menu_presentation_delegate.h"

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

@end

// Tab container which positions the toolbar at the top.
@interface TopToolbarTabViewController : TabContainerViewController
@end

// Tab container which positions the toolbar at the bottom.
@interface BottomToolbarTabViewController : TabContainerViewController
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_CONTAINER_VIEW_CONTROLLER_H_
