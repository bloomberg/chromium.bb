// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MANAGE_CREDENTIALS_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MANAGE_CREDENTIALS_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/passwords/credential_item_view.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_content_view_controller.h"

@class AccountAvatarFetcherManager;
@class ManageCredentialItemViewController;
class ManagePasswordsBubbleModel;

// A custom cell that displays a credential item in an NSTableView.
@interface ManageCredentialItemCell : NSCell
- (id)initWithItem:(ManageCredentialItemViewController*)item;
@property(nonatomic, readonly) ManageCredentialItemViewController* item;
@end

// Manages a view that shows a list of credentials that can be used for
// authentication to a site.
@interface ManagePasswordsBubbleManageCredentialsViewController
    : ManagePasswordsBubbleContentViewController<CredentialItemDelegate,
                                                 NSTableViewDataSource,
                                                 NSTableViewDelegate> {
 @private
  ManagePasswordsBubbleModel* model_;  // Weak.
  base::scoped_nsobject<NSButton> manageButton_;
  base::scoped_nsobject<NSButton> doneButton_;
  base::scoped_nsobject<NSTableView> credentialsView_;
  base::scoped_nsobject<NSArray> credentialItems_;
  base::scoped_nsobject<AccountAvatarFetcherManager> avatarManager_;
}

// Initializes a new credentials manager bubble and populates it with the
// credentials stored in |model|.
- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate;

@end

@interface ManagePasswordsBubbleManageCredentialsViewController (Testing)
@property(nonatomic, readonly) NSButton* manageButton;
@property(nonatomic, readonly) NSButton* doneButton;
@property(nonatomic, readonly) NSTableView* credentialsView;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MANAGE_CREDENTIALS_VIEW_CONTROLLER_H_
