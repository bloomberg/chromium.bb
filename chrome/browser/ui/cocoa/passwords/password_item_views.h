// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORD_ITEM_VIEWS_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORD_ITEM_VIEWS_H_

#import <Cocoa/Cocoa.h>

namespace autofill {
struct PasswordForm;
}  // namespace autofill

@class HoverImageButton;
@protocol PasswordItemDelegate;

// The state of the password item.
enum ManagePasswordItemState {
  MANAGE_PASSWORD_ITEM_STATE_PENDING,
  MANAGE_PASSWORD_ITEM_STATE_MANAGE,
  MANAGE_PASSWORD_ITEM_STATE_DELETED
};

// Protocol for items that have two dynamic columns.
@protocol PasswordItemTwoColumnView<NSObject>
// Resize the subelements according to cummulative column's sizes across all the
// rows.
- (void)layoutWithFirstColumn:(CGFloat)firstWidth
                 secondColumn:(CGFloat)secondWidth;

@property(readonly, nonatomic) CGFloat firstColumnWidth;
@property(readonly, nonatomic) CGFloat secondColumnWidth;
@end

// Shows the option to undelete a password.
@interface UndoPasswordItemView : NSView<PasswordItemTwoColumnView> {
 @private
  base::scoped_nsobject<NSTextField> label_;
  base::scoped_nsobject<NSButton> undoButton_;
}
- (id)initWithTarget:(id)target action:(SEL)action;
@end

@interface UndoPasswordItemView (Testing)
@property(readonly) NSButton* undoButton;
@end

// Shows a username, obscured password, and delete button in a single row.
@interface ManagePasswordItemView : NSView<PasswordItemTwoColumnView> {
 @private
  base::scoped_nsobject<NSTextField> usernameField_;
  // The field contains the password or IDP origin for federated credentials.
  base::scoped_nsobject<NSTextField> passwordField_;
  base::scoped_nsobject<HoverImageButton> deleteButton_;
}
- (id)initWithForm:(const autofill::PasswordForm&)form
            target:(id)target
            action:(SEL)action;
@end

@interface ManagePasswordItemView (Testing)
@property(readonly) NSTextField* usernameField;
@property(readonly) NSTextField* passwordField;
@property(readonly) NSButton* deleteButton;
@end

// Shows a username and obscured password in a single row.
@interface PendingPasswordItemView : NSView<PasswordItemTwoColumnView> {
 @private
  base::scoped_nsobject<NSTextField> usernameField_;
  // The field contains the password or IDP origin for federated credentials.
  base::scoped_nsobject<NSTextField> passwordField_;
}
@property(readonly, nonatomic) NSTextField* usernameField;

- (id)initWithForm:(const autofill::PasswordForm&)form;
@end

@interface PendingPasswordItemView (Testing)
@property(readonly) NSSecureTextField* passwordField;
@end

// Shows a single item in a password management list. Transitions between
// PENDING, MANAGE, and DELETED states according to user interaction.
@interface ManagePasswordItemViewController
    : NSViewController<PasswordItemTwoColumnView> {
 @private
  id<PasswordItemDelegate> delegate_;  // weak
  const autofill::PasswordForm* passwordForm_;
  ManagePasswordItemState state_;
  base::scoped_nsobject<NSView<PasswordItemTwoColumnView>> contentView_;
}
- (id)initWithDelegate:(id<PasswordItemDelegate>)delegate
          passwordForm:(const autofill::PasswordForm*)passwordForm;
@end

@interface ManagePasswordItemViewController (Testing)
@property(readonly) ManagePasswordItemState state;
@property(readonly) NSView* contentView;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORD_ITEM_VIEWS_H_
