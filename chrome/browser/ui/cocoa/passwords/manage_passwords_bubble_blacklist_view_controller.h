// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_BLACKLIST_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_BLACKLIST_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_content_view_controller.h"

@class BubbleCombobox;
class ManagePasswordsBubbleModel;
@class ManagePasswordItemViewController;

// Manages the view that informs the user that the current site is blacklisted
// from the password manager and offers to un-blacklist it.
@interface ManagePasswordsBubbleBlacklistViewController
    : ManagePasswordsBubbleContentViewController {
 @private
  ManagePasswordsBubbleModel* model_;  // weak
  id<ManagePasswordsBubbleContentViewDelegate> delegate_;  // weak
  base::scoped_nsobject<NSButton> doneButton_;
  base::scoped_nsobject<NSButton> undoBlacklistButton_;
}
- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate;
@end

@interface ManagePasswordsBubbleBlacklistViewController (Testing)
@property(readonly) NSButton* doneButton;
@property(readonly) NSButton* undoBlacklistButton;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_BLACKLIST_VIEW_CONTROLLER_H_
