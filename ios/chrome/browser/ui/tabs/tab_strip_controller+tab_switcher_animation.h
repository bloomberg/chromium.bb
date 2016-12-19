// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_TAB_SWITCHER_ANIMATION_H_
#define IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_TAB_SWITCHER_ANIMATION_H_

#include "base/ios/block_types.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_controller.h"

@class TabSwitcherTabStripPlaceholderView;

// This category is used by the tab switcher to start fold/unfold animations
// on the tab strip controller.
@interface TabStripController (TabSwitcherAnimation)

// Returns a tab switcher tab strip placeholder view created from the current
// state of the tab strip controller. It is used to animate the transition
// from the browser view controller to the tab switcher.
- (TabSwitcherTabStripPlaceholderView*)placeholderView;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_TAB_SWITCHER_ANIMATION_H_
