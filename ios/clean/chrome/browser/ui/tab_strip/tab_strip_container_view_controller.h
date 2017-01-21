// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/animators/zoom_transition_delegate.h"
#import "ios/clean/chrome/browser/ui/presenters/menu_presentation_delegate.h"

// Base class for a view controller that contains a content view (generally a
// web view with toolbar, but nothing in this class assumes that) and a strip
// view, each managed by their own view controllers.
@interface TabStripContainerViewController
    : UIViewController<MenuPresentationDelegate, ZoomTransitionDelegate>

// View controller showing the main content. If there is no strip view
// controller set, the contents of this view controller will fill all of the
// strip container's view.
@property(nonatomic, strong) UIViewController* contentViewController;

// View controller showing the strip. It will be of a fixed
// height (determined internally by the strip container), but will span the
// width of the tab.
@property(nonatomic, strong) UIViewController* tabStripViewController;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_CONTAINER_VIEW_CONTROLLER_H_
