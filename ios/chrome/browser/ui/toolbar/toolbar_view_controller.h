// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/animators/zoom_transition_delegate.h"

@protocol ToolbarCommands;

// View controller for a toolbar, which will show a horizontal row of
// controls and/or labels.
// This view controller will fill its container; it is up to the containing
// view controller or presentation controller to configure an appropriate
// height for it.
@interface ToolbarViewController : UIViewController<ZoomTransitionDelegate>

// The action delegate for this view controller.
@property(nonatomic, weak) id<ToolbarCommands> toolbarCommandHandler;

// Sets the text for a label appearing in the center of the toolbar.
- (void)setCurrentPageText:(NSString*)text;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_VIEW_CONTROLLER_H_
