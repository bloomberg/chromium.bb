// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#import "ui/base/cocoa/tracking_area.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

@class HoverImageButton;
class ManagePasswordsBubbleModel;

// The state of the password item.
enum ManagePasswordItemState {
  MANAGE_PASSWORD_ITEM_STATE_PENDING,
  MANAGE_PASSWORD_ITEM_STATE_MANAGE,
  MANAGE_PASSWORD_ITEM_STATE_DELETED
};

// Abstract superclass for items that are clickable. Highlights on hover.
@interface ManagePasswordItemClickableView : NSView {
 @private
  BOOL hovering_;
  ui::ScopedCrTrackingArea trackingArea_;
}
@end

// Shows the option to undelete a password.
@interface ManagePasswordItemUndoView : ManagePasswordItemClickableView {
 @private
  base::scoped_nsobject<NSButton> undoButton_;
}
- (id)initWithTarget:(id)target action:(SEL)action;
@end

@interface ManagePasswordItemUndoView (Testing)
@property(readonly) NSButton* undoButton;
@end

// Shows a username, obscured password, and delete button in a single row.
@interface ManagePasswordItemManageView : ManagePasswordItemClickableView {
 @private
  base::scoped_nsobject<NSTextField> usernameField_;
  base::scoped_nsobject<NSSecureTextField> passwordField_;
  base::scoped_nsobject<HoverImageButton> deleteButton_;
}
- (id)initWithForm:(const autofill::PasswordForm&)form
            target:(id)target
            action:(SEL)action;
@end

@interface ManagePasswordItemManageView (Testing)
@property(readonly) NSTextField* usernameField;
@property(readonly) NSSecureTextField* passwordField;
@property(readonly) NSButton* deleteButton;
@end

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
  autofill::PasswordForm passwordForm_;
  ManagePasswordItemState state_;
  password_manager::ui::PasswordItemPosition position_;
  base::scoped_nsobject<NSView> contentView_;
}
- (id)initWithModel:(ManagePasswordsBubbleModel*)model
       passwordForm:(const autofill::PasswordForm&)passwordForm
           position:(password_manager::ui::PasswordItemPosition)position;
@end

@interface ManagePasswordItemViewController (Testing)
@property(readonly) ManagePasswordItemState state;
@property(readonly) NSView* contentView;
@property(readonly) autofill::PasswordForm passwordForm;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_CONTROLLER_H_
