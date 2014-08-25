// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_CONFIRMATION_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_CONFIRMATION_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_content_view_controller.h"

@class HyperlinkTextView;
class ManagePasswordsBubbleModel;

// Manages the view that confirms that the generated password was saved.
@interface ManagePasswordsBubbleConfirmationViewController
    : ManagePasswordsBubbleContentViewController<NSTextViewDelegate> {
 @private
  ManagePasswordsBubbleModel* model_;  // weak
  id<ManagePasswordsBubbleContentViewDelegate> delegate_;  // weak
  base::scoped_nsobject<HyperlinkTextView> confirmationText_;
  base::scoped_nsobject<NSButton> okButton_;
}
- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate;
@end

@interface ManagePasswordsBubbleConfirmationViewController (Testing)
@property(readonly) HyperlinkTextView* confirmationText;
@property(readonly) NSButton* okButton;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_CONFIRMATION_VIEW_CONTROLLER_H_
