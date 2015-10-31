// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/passwords/base_passwords_content_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/credential_item_view.h"

@class AccountAvatarFetcherManager;
@class BubbleCombobox;
class ManagePasswordsBubbleModel;
@class ManagePasswordCredentialItemViewController;

// A custom cell that displays a credential item in an NSTableView.
@interface CredentialItemCell : NSCell
- (id)initWithView:(CredentialItemView*)view;
@property(nonatomic, readonly) CredentialItemView* view;
@end

// Manages a view that shows a list of credentials that can be used for
// authentication to a site.
@interface ManagePasswordsBubbleAccountChooserViewController
    : ManagePasswordsBubbleContentViewController<CredentialItemDelegate,
                                                 NSTableViewDataSource,
                                                 NSTableViewDelegate> {
 @private
  ManagePasswordsBubbleModel* model_;  // Weak.
  NSButton* cancelButton_;  // Weak.
  BubbleCombobox* moreButton_;  // Weak.
  NSTableView* credentialsView_;  // Weak.
  base::scoped_nsobject<NSArray> credentialItems_;
  base::scoped_nsobject<AccountAvatarFetcherManager> avatarManager_;
}

// Initializes a new account chooser and populates it with the credentials
// stored in |model|. Designated initializer.
- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_
