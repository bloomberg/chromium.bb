// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_CONTAINER_VIEW_CONTROLLER_INTERNAL_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_CONTAINER_VIEW_CONTROLLER_INTERNAL_H_

#import "ios/clean/chrome/browser/ui/tab/tab_container_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/ui_types.h"

// Internal API for subclasses and categories of TabContainerViewController.
@interface TabContainerViewController (Internal)

// Container view that holds the toolbar, findbar, and content view.
// Subclasses should arrange any additional views along with |containerView|.
@property(nonatomic, readonly) UIView* containerView;

// Helper method for attaching a child view controller.
- (void)attachChildViewController:(UIViewController*)viewController
                        toSubview:(UIView*)subview;

// Helper method for detaching a child view controller.
- (void)detachChildViewController:(UIViewController*)viewController;

// Configures all the subviews. Subclasses may override this method to configure
// additional subviews. Subclass overrides must first call the super method.
- (void)configureSubviews;

// Returns constraints for the placement of the container. Subclasses
// may override this method to arrange additional subviews along with the
// container.
- (Constraints*)containerConstraints;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_CONTAINER_VIEW_CONTROLLER_INTERNAL_H_
