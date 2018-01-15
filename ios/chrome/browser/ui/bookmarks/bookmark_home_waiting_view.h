// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_WAITING_VIEW_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_WAITING_VIEW_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"

// Displays a waiting UI for BookmarkHomeHandsetController.
// It displays a gray background and an activity indicator.
// The activity indicator appears after a delay, starting from the moment
// |startWaiting| is called.
@interface BookmarkHomeWaitingView : UIView

- (instancetype)initWithFrame:(CGRect)frame backgroundColor:(UIColor*)color;

// Call this method when this view is added to the visible view hierarchy.
// After a delay, an activity indicator will be presented if this view is still
// in the view hierarchy at that time.
- (void)startWaiting;

// Call this method when this view is removed from the visible view hierarchy.
// |completion| will be called when this view is done animating out.
- (void)stopWaitingWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_WAITING_VIEW_H_
