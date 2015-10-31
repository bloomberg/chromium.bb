// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/passwords/base_passwords_content_view_controller.h"

class ManagePasswordsBubbleModel;
@class PasswordsListViewController;

// Informs the user that no passwords are stored for the current site.
@interface NoPasswordsView : NSTextField
- (id)initWithWidth:(CGFloat)width;
@end

// Manages the view that allows users to manage passwords for a site.
@interface ManagePasswordsBubbleManageViewController
    : ManagePasswordsBubbleContentViewController {
 @private
  ManagePasswordsBubbleModel* model_;  // weak
  base::scoped_nsobject<NSButton> doneButton_;
  base::scoped_nsobject<NSButton> manageButton_;
  base::scoped_nsobject<NoPasswordsView> noPasswordsView_;
  base::scoped_nsobject<PasswordsListViewController> passwordsListController_;
}
- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate;
@end

@interface ManagePasswordsBubbleManageViewController (Testing)
@property(readonly) NSButton* doneButton;
@property(readonly) NSButton* manageButton;
@property(readonly) NoPasswordsView* noPasswordsView;
@property(readonly) PasswordsListViewController* passwordsListController;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_VIEW_CONTROLLER_H_
