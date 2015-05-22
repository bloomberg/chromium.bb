// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_SAVE_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_SAVE_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_content_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_pending_view_controller.h"

@class BubbleCombobox;
class ManagePasswordsBubbleModel;

// Manages the view that allows users to save an account when using the new
// credentials-based UI.
@interface ManagePasswordsBubbleSaveViewController
    : ManagePasswordsBubbleContentViewController {
  ManagePasswordsBubbleModel* model_;  // Weak.
  base::scoped_nsobject<NSButton> saveButton_;
  base::scoped_nsobject<NSButton> noThanksButton_;
  base::scoped_nsobject<BubbleCombobox> moreButton_;
}
- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubblePendingViewDelegate>)delegate;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_SAVE_VIEW_CONTROLLER_H_
