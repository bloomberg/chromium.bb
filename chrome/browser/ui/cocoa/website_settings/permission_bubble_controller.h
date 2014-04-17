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
  // Only populated if |customizationMode| is YES when the UI is shown.
  base::scoped_nsobject<NSMutableArray> checkboxes_;

  // Delegate to be informed of user actions.
  PermissionBubbleView::Delegate* delegate_;  // Weak.

  // Delegate that receives menu events on behalf of this.
  scoped_ptr<ui::SimpleMenuModel::Delegate> menuDelegate_;

  // Bridge to the C++ class that created this object.
  PermissionBubbleCocoa* bridge_;  // Weak.
}

// Designated initializer.  |parentWindow| and |bridge| must both be non-nil.
- (id)initWithParentWindow:(NSWindow*)parentWindow
                    bridge:(PermissionBubbleCocoa*)bridge;

// Makes the bubble visible, with an arrow pointing to |anchor|.  The bubble
// will be populated with text retrieved from |requests|.  If
// |customizationMode| is YES, each request will have a checkbox, with its state
// set to the corresponding element in |acceptStates|.  If it is NO, each
// request will have a bullet point and |acceptStates| may be empty.  |delegate|
// will receive callbacks for user actions.
- (void)showAtAnchor:(NSPoint)anchor
         withDelegate:(PermissionBubbleView::Delegate*)delegate
          forRequests:(const std::vector<PermissionBubbleRequest*>&)requests
         acceptStates:(const std::vector<bool>&)acceptStates
    customizationMode:(BOOL)customizationMode;

// Called when a menu item is selected.
- (void)onMenuItemClicked:(int)commandId;

@end
