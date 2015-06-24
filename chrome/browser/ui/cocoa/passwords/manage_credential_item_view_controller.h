// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_CREDENTIAL_ITEM_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_CREDENTIAL_ITEM_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "components/autofill/core/common/password_form.h"

@protocol CredentialItemDelegate;
@class CredentialItemView;
@class HoverImageButton;
class ManagePasswordsBubbleModel;

@interface ManageCredentialItemView : NSView {
  base::scoped_nsobject<CredentialItemView> credentialItem_;
  base::scoped_nsobject<HoverImageButton> deleteButton_;
}
- (id)initWithPasswordForm:(const autofill::PasswordForm&)passwordForm
                  delegate:(id<CredentialItemDelegate>)delegate
                    target:(id)target
                    action:(SEL)action;
@end

@interface DeletedCredentialItemView : NSView {
  base::scoped_nsobject<NSButton> undoButton_;
}
- (id)initWithTarget:(id)target action:(SEL)action;
@end

@interface ManageCredentialItemViewController : NSViewController {
  base::scoped_nsobject<NSView> contentView_;
  autofill::PasswordForm passwordForm_;
  ManagePasswordsBubbleModel* model_;
  id<CredentialItemDelegate> delegate_;  // Weak.
}
- (id)initWithPasswordForm:(const autofill::PasswordForm&)passwordForm
                     model:(ManagePasswordsBubbleModel*)model
                  delegate:(id<CredentialItemDelegate>)delegate;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_CREDENTIAL_ITEM_VIEW_CONTROLLER_H_
