// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_PENDING_PASSWORD_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_PENDING_PASSWORD_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/passwords/base_passwords_content_view_controller.h"

class ManagePasswordsBubbleModel;
@class PasswordsListViewController;

// Manages the view that offers to save the user's password.
@interface ManagePasswordsBubblePendingViewController
    : ManagePasswordsBubbleContentViewController<NSTextViewDelegate> {
 @private
  ManagePasswordsBubbleModel* model_;  // weak
  base::scoped_nsobject<NSButton> saveButton_;
  base::scoped_nsobject<NSButton> neverButton_;
  base::scoped_nsobject<NSButton> closeButton_;
  base::scoped_nsobject<PasswordsListViewController> passwordItem_;
}
- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate;
@end

@interface ManagePasswordsBubblePendingViewController (Testing)
@property(readonly) NSButton* saveButton;
@property(readonly) NSButton* neverButton;
@property(readonly) NSButton* closeButton;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_PENDING_PASSWORD_VIEW_CONTROLLER_H_
