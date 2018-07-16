// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_GRID_TO_TAB_TRANSITION_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_GRID_TO_TAB_TRANSITION_VIEW_H_

#import <UIKit/UIKit.h>

// A protocol to be adopted by a view that will be provided for the transition
// animation. This view will need to be animated between 'cell' and 'tab'
// states, and will need to have a number of properties available for that
// purpose.
@protocol GridToTabTransitionView

// The subview at the top of the view in 'cell' state.
@property(nonatomic, strong) UIView* topCellView;

// The corner radius of the view.
@property(nonatomic) CGFloat cornerRadius;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_GRID_TO_TAB_TRANSITION_VIEW_H_
