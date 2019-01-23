// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_EMPTY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_EMPTY_VIEW_H_

#import <UIKit/UIKit.h>

// Protocol defining the interface of the view displayed when the grid is empty.
@protocol GridEmptyView

// Insets of the inner ScrollView.
@property(nonatomic, assign) UIEdgeInsets scrollViewContentInsets;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_EMPTY_VIEW_H_
