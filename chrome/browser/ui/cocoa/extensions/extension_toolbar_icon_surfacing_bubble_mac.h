// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_TOOLBAR_ICON_SURFACING_BUBBLE_MAC_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_TOOLBAR_ICON_SURFACING_BUBBLE_MAC_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

class ToolbarActionsBarBubbleDelegate;

// A bubble for a short user education for the new extension toolbar.
@interface ExtensionToolbarIconSurfacingBubbleMac : BaseBubbleController {
  // Whether or not the bubble has been acknowledged.
  BOOL acknowledged_;

  ToolbarActionsBarBubbleDelegate* delegate_;
}

// Creates the bubble for a parent window but does not show it.
- (id)initWithParentWindow:(NSWindow*)parentWindow
               anchorPoint:(NSPoint)anchorPoint
                  delegate:(ToolbarActionsBarBubbleDelegate*)delegate;

@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_TOOLBAR_ICON_SURFACING_BUBBLE_MAC_H_
