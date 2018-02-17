// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_TRANSITION_STATE_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_TRANSITION_STATE_PROVIDER_H_

#import <Foundation/Foundation.h>

// Objects conforming to this protocol can provide state information to
// transition delegates and animators for the tab grid.
@protocol TabGridTransitionStateProvider

// YES if the currently selected tab is visible in the tab grid.
@property(nonatomic, readonly) BOOL selectedTabVisible;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_TRANSITION_STATE_PROVIDER_H_
