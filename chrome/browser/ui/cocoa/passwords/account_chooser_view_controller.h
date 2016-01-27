// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/passwords/credential_item_view.h"

@class AccountAvatarFetcherManager;

class PasswordDialogController;

namespace net {
class URLRequestContextGetter;
}

// A custom cell that displays a credential item in an NSTableView.
@interface CredentialItemCell : NSCell
- (id)initWithView:(CredentialItemView*)view;
@property(nonatomic, readonly) CredentialItemView* view;
@end

// An interface for the bridge between AccountChooserViewController and platform
// independent UI code.
class AccountChooserBridge {
 public:
  // Closes the dialog.
  virtual void PerformClose() = 0;

  // Returns the controller containing the data.
  virtual PasswordDialogController* GetDialogController() = 0;

  // Returns the request context for fetching the avatars.
  virtual net::URLRequestContextGetter* GetRequestContext() const = 0;

 protected:
  virtual ~AccountChooserBridge() = default;
};

// Manages a view that shows a list of credentials that can be used for
// authentication to a site.
@interface AccountChooserViewController
    : NSViewController<CredentialItemDelegate,
                       NSTableViewDataSource,
                       NSTableViewDelegate,
                       NSTextViewDelegate> {
 @private
  AccountChooserBridge* bridge_;  // Weak.
  NSButton* cancelButton_;  // Weak.
  NSTableView* credentialsView_;  // Weak.
  NSTextView* titleView_;  //  Weak.
  base::scoped_nsobject<NSArray> credentialItems_;
  base::scoped_nsobject<AccountAvatarFetcherManager> avatarManager_;
}

// Initializes a new account chooser and populates it with the credentials
// stored in |bridge->controller()|.
- (instancetype)initWithBridge:(AccountChooserBridge*)bridge;

@property(nonatomic) AccountChooserBridge* bridge;
@end

@interface AccountChooserViewController(Testing)
- (instancetype)initWithBridge:(AccountChooserBridge*)bridge
                 avatarManager:(AccountAvatarFetcherManager*)avatarManager;
@property(nonatomic, readonly) NSButton* cancelButton;
@property(nonatomic, readonly) NSTableView* credentialsView;
@property(nonatomic, readonly) NSTextView* titleView;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_
