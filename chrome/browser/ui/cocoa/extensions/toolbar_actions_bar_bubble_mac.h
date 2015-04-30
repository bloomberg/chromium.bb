// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_TOOLBAR_ACTIONS_BAR_BUBBLE_MAC_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_TOOLBAR_ACTIONS_BAR_BUBBLE_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

class ToolbarActionsBarBubbleDelegate;

// A bubble that hangs off the toolbar actions bar, with a minimum of a heading,
// some text content, and an "action" button. The bubble can also, optionally,
// have a learn more link and/or a dismiss button.
@interface ToolbarActionsBarBubbleMac : BaseBubbleController {
  // Whether or not the bubble has been acknowledged.
  BOOL acknowledged_;

  // The action button. The exact meaning of this is dependent on the bubble.
  // Required.
  NSButton* actionButton_;

  // The list of items to display. Optional.
  NSTextField* itemList_;

  // The dismiss button. Optional.
  NSButton* dismissButton_;

  // The "learn more" link-style button. Optional.
  NSButton* learnMoreButton_;

  // This bubble's delegate.
  scoped_ptr<ToolbarActionsBarBubbleDelegate> delegate_;
}

// Creates the bubble for a parent window but does not show it.
- (id)initWithParentWindow:(NSWindow*)parentWindow
               anchorPoint:(NSPoint)anchorPoint
                  delegate:(scoped_ptr<ToolbarActionsBarBubbleDelegate>)
                               delegate;

// Toggles animation for testing purposes.
+ (void)setAnimationEnabledForTesting:(BOOL)enabled;

@property(readonly, nonatomic) NSButton* actionButton;
@property(readonly, nonatomic) NSTextField* itemList;
@property(readonly, nonatomic) NSButton* dismissButton;
@property(readonly, nonatomic) NSButton* learnMoreButton;

@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_TOOLBAR_ACTIONS_BAR_BUBBLE_MAC_H_
