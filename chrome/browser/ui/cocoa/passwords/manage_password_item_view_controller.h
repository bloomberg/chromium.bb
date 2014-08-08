// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "components/password_manager/core/common/password_manager_ui.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

class ManagePasswordsBubbleModel;

// The state of the password item.
enum ManagePasswordItemState {
  MANAGE_PASSWORD_ITEM_STATE_PENDING,
  MANAGE_PASSWORD_ITEM_STATE_MANAGE,
  MANAGE_PASSWORD_ITEM_STATE_DELETED
};

// Shows a username and obscured password in a single row.
@interface ManagePasswordItemPendingView : NSView {
 @private
  base::scoped_nsobject<NSTextField> usernameField_;
  base::scoped_nsobject<NSSecureTextField> passwordField_;
}
- (id)initWithForm:(const autofill::PasswordForm&)form;
@end

@interface ManagePasswordItemPendingView (Testing)
@property(readonly) NSTextField* usernameField;
@property(readonly) NSSecureTextField* passwordField;
@end

// Shows a single item in a password management list. Transitions between
// PENDING, MANAGE, and DELETED states according to user interaction.
@interface ManagePasswordItemViewController : NSViewController {
 @private
  ManagePasswordsBubbleModel* model_;  // weak
  ManagePasswordItemState state_;
  password_manager::ui::PasswordItemPosition position_;
  base::scoped_nsobject<NSView> contentView_;
  CGFloat minWidth_;
}
- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           position:(password_manager::ui::PasswordItemPosition)position
           minWidth:(CGFloat)minWidth;
@end

@interface ManagePasswordItemViewController (Testing)
@property(readonly) ManagePasswordItemState state;
@property(readonly) NSView* contentView;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_CONTROLLER_H_
