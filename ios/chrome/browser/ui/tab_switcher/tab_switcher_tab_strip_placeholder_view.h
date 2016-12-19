// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_TAB_STRIP_PLACEHOLDER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_TAB_STRIP_PLACEHOLDER_VIEW_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"

// Used has a placeholder for the tab strip view during the tab switcher
// controller transition animations.
@interface TabSwitcherTabStripPlaceholderView : UIView

// Triggers a fold animation of the tab strip placeholder resulting
// in hiding all the tabs views. |completion| is called at the end of the
// animation.
- (void)foldWithCompletion:(ProceduralBlock)completion;

// Triggers an unfold animation of the tab strip placeholder resulting in
// showing the tabs views. |completion| is called at the end of the animation.
- (void)unfoldWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_TAB_STRIP_PLACEHOLDER_VIEW_H_
