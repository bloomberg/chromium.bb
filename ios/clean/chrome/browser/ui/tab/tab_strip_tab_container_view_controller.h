// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_STRIP_TAB_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_STRIP_TAB_CONTAINER_VIEW_CONTROLLER_H_

#import "ios/clean/chrome/browser/ui/tab/tab_container_view_controller.h"

#import <UIKit/UIKit.h>

// Tab container that has a tab strip in addition to all the surfaces in the
// basic tab container.
@interface TabStripTabContainerViewController : TabContainerViewController

// View controller showing the tab strip. It will be of a fixed
// height (determined internally by the tab container), but will span the
// width of the tab.
@property(nonatomic, strong) UIViewController* tabStripViewController;

// YES if |tabStripViewController| is currently visible.
@property(nonatomic, assign, getter=isTabStripVisible) BOOL tabStripVisible;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_STRIP_TAB_CONTAINER_VIEW_CONTROLLER_H_
