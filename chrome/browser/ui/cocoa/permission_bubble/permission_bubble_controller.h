// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PERMISSION_BUBBLE_PERMISSION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PERMISSION_BUBBLE_PERMISSION_BUBBLE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;
class LocationBarDecoration;
@class MenuController;
class PermissionBubbleCocoa;

@interface PermissionBubbleController
    : BaseBubbleController<NSTextViewDelegate> {
 @private
  // Array of views that are the checkboxes for every requested permission.
  // Only populated if multiple requests are shown at once.
  base::scoped_nsobject<NSMutableArray> checkboxes_;

  // Delegate to be informed of user actions.
  PermissionPrompt::Delegate* delegate_;  // Weak.

  // Used to determine the correct anchor location and parent window.
  Browser* browser_;  // Weak.

  // Delegate that receives menu events on behalf of this.
  std::unique_ptr<ui::SimpleMenuModel::Delegate> menuDelegate_;

  // Bridge to the C++ class that created this object.
  PermissionBubbleCocoa* bridge_;  // Weak.

  // The omnibox icon the bubble is anchored to. The icon is set as active
  // when the bubble is opened, and inactive when the bubble is closed.
  // Usually we would override OmniboxDecorationBubbleController but the page
  // info icon has a special case where it might cause a race condition.
  LocationBarDecoration* decoration_;  // Weak.
}

// Designated initializer.  |browser| and |bridge| must both be non-nil.
- (id)initWithBrowser:(Browser*)browser bridge:(PermissionBubbleCocoa*)bridge;

// Returns the anchor point to use for the given Cocoa |browser|.
+ (NSPoint)getAnchorPointForBrowser:(Browser*)browser;

// Returns true if |browser| has a visible location bar.
+ (bool)hasVisibleLocationBarForBrowser:(Browser*)browser;

// Makes the bubble visible. The bubble will be populated with text retrieved
// from |delegate|, which will also receive callbacks for user actions.
- (void)showWithDelegate:(PermissionPrompt::Delegate*)delegate;

// Will reposition the bubble based in case the anchor or parent should change.
- (void)updateAnchorPosition;

// Will calculate the expected anchor point for this bubble.
// Should only be used outside this class for tests.
- (NSPoint)getExpectedAnchorPoint;

// Returns true if the browser has a visible location bar.
// Should only be used outside this class for tests.
- (bool)hasVisibleLocationBar;

@end

#endif  // CHROME_BROWSER_UI_COCOA_PERMISSION_BUBBLE_PERMISSION_BUBBLE_CONTROLLER_H_
