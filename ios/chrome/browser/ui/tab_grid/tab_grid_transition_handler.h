// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_TRANSITION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_TRANSITION_HANDLER_H_

#import <UIKit/UIKit.h>

@protocol TabGridTransitionStateProvider;

@interface TabGridTransitionHandler
    : NSObject<UIViewControllerTransitioningDelegate>

@property(nonatomic, weak) id<TabGridTransitionStateProvider> provider;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_TRANSITION_HANDLER_H_
