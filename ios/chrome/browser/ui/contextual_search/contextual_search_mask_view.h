// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_MASK_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_MASK_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/contextual_search/contextual_search_panel_protocols.h"

@interface ContextualSearchMaskView
    : UIView<ContextualSearchPanelMotionObserver>

// Create a mask view. It will szie itself to cover its superview, and will
// adjust its opacity in response to the motion of the panel it observes.

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_MASK_VIEW_H_
