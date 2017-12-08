// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_VIEW_H_

#import <UIKit/UIKit.h>

// TODO(crbug.com/792440): Once the new notification for fullscreen is
// completed, this protocol can be removed. Protocol handling the fullscreen for
// the Toolbar.
@protocol ToolbarViewFullscreenDelegate
// Called when the frame of the Toolbar View has changed.
- (void)toolbarViewFrameChanged;
@end

// The view displaying the toolbar.
@interface ToolbarView : UIView

// The delegate used to handle frame changes.
@property(nonatomic, weak) id<ToolbarViewFullscreenDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_VIEW_H_
