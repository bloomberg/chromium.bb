// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_CONTAINER_VIEW_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

// Container view class that manages the display of content within
// CRWWebController.
@interface CRWWebControllerContainerView : UIView

// |toolbar| will be resized to the container width, bottom-aligned, and added
// as the topmost subview.
- (void)addToolbar:(UIView*)toolbar;

// Adds each toolbar in |toolbars|.
- (void)addToolbars:(NSArray*)toolbars;

// Removes |toolbar| as a subview.
- (void)removeToolbar:(UIView*)toolbar;

// Removes all toolbars added via |-addToolbar:|.
- (void)removeAllToolbars;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_CONTAINER_VIEW_H_
