// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#include "chrome/browser/ui/website_settings/permission_bubble_view.h"
#include "ui/base/models/simple_menu_model.h"

@class MenuController;
class PermissionBubbleCocoa;
class PermissionBubbleRequest;

@interface PermissionBubbleController :
    BaseBubbleController<NSTextViewDelegate> {
 @private
  // Array of views that are the checkboxes for every requested permission.
  // Only populated if multiple requests are shown at once.
  base::scoped_nsobject<NSMutableArray> checkboxes_;

  // Delegate to be informed of user actions.
  PermissionBubbleView::Delegate* delegate_;  // Weak.

  // Used to determine the correct anchor location and parent window.
  Browser* browser_;  // Weak.

  // Delegate that receives menu events on behalf of this.
  std::unique_ptr<ui::SimpleMenuModel::Delegate> menuDelegate_;

  // Bridge to the C++ class that created this object.
  PermissionBubbleCocoa* bridge_;  // Weak.
}

// Designated initializer.  |browser| and |bridge| must both be non-nil.
- (id)initWithBrowser:(Browser*)browser bridge:(PermissionBubbleCocoa*)bridge;

// Makes the bubble visible. The bubble will be popuplated with text retrieved
// from |requests|. |delegate| will receive callbacks for user actions.
- (void)showWithDelegate:(PermissionBubbleView::Delegate*)delegate
             forRequests:(const std::vector<PermissionBubbleRequest*>&)requests
            acceptStates:(const std::vector<bool>&)acceptStates;

// Will reposition the bubble based in case the anchor or parent should change.
- (void)updateAnchorPosition;

// Will calculate the expected anchor point for this bubble.
// Should only be used outside this class for tests.
- (NSPoint)getExpectedAnchorPoint;

// Returns true if the browser has a visible location bar.
// Should only be used outside this class for tests.
- (bool)hasVisibleLocationBar;

@end
