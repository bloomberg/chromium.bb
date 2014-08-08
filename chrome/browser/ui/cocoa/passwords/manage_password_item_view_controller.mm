// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_password_item_view_controller.h"

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/layout/layout_constants.h"

namespace {

// TODO(dconnelly): Figure out how to share all these constants.
static const CGFloat kBorderWidth = 1;
static const CGFloat kFramePadding = 16;

void InitLabel(NSTextField* textField, const base::string16& text) {
  [textField setStringValue:base::SysUTF16ToNSString(text)];
  [textField setEditable:NO];
  [textField setSelectable:NO];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];
  NSFont* font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
  [textField setFont:font];
  [textField sizeToFit];
  // TODO(dconnelly): Handle max width.
}

NSTextField* UsernameLabel(const base::string16& text) {
  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  InitLabel(textField, text);
  return textField.autorelease();
}

NSSecureTextField* PasswordLabel(const base::string16& text) {
  base::scoped_nsobject<NSSecureTextField> textField(
      [[NSSecureTextField alloc] initWithFrame:NSZeroRect]);
  InitLabel(textField, text);
  return textField.autorelease();
}

}  // namespace

@implementation ManagePasswordItemPendingView

- (id)initWithForm:(const autofill::PasswordForm&)form {
  if ((self = [super initWithFrame:NSZeroRect])) {
    CGFloat curX = 0;
    CGFloat curY = 0;

    // Add the username.
    usernameField_.reset([UsernameLabel(form.username_value) retain]);
    [usernameField_ setFrameOrigin:NSMakePoint(curX, curY)];
    [self addSubview:usernameField_];

    // Move to the right of the username and add the password.
    curX += NSWidth([usernameField_ frame]) + views::kItemLabelSpacing;
    passwordField_.reset([PasswordLabel(form.password_value) retain]);
    [passwordField_ setFrameOrigin:NSMakePoint(curX, curY)];
    [self addSubview:passwordField_];

    // Move to the top-right of the password.
    curX = NSMaxX([passwordField_ frame]);
    curY = NSMaxY([passwordField_ frame]);

    // Update the frame.
    [self setFrameSize:NSMakeSize(curX, curY)];
  }
  return self;
}

@end

@implementation ManagePasswordItemPendingView (Testing)

- (NSTextField*)usernameField {
  return usernameField_.get();
}

- (NSSecureTextField*)passwordField {
  return passwordField_.get();
}

@end

@implementation ManagePasswordItemViewController

- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           position:(password_manager::ui::PasswordItemPosition)position
           minWidth:(CGFloat)minWidth {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    model_ = model;
    position_ = position;
    minWidth_ = minWidth;
    state_ = password_manager::ui::IsPendingState(model_->state())
        ? MANAGE_PASSWORD_ITEM_STATE_PENDING
        : MANAGE_PASSWORD_ITEM_STATE_MANAGE;
    switch (state_) {
      default:
        NOTREACHED();
      case MANAGE_PASSWORD_ITEM_STATE_PENDING:
        contentView_.reset([[ManagePasswordItemPendingView alloc]
            initWithForm:model_->pending_credentials()]);
        break;
      case MANAGE_PASSWORD_ITEM_STATE_MANAGE:
        NOTIMPLEMENTED();
        break;
      case MANAGE_PASSWORD_ITEM_STATE_DELETED:
        NOTIMPLEMENTED();
        break;
    };
  }
  return self;
}

- (void)loadView {
  self.view = [[[NSView alloc] initWithFrame:NSZeroRect] autorelease];
  [self.view addSubview:contentView_];

  // Update the view size according to the content view size, expanding if
  // necessary to fill the min width.
  const NSSize contentSize = [contentView_ frame].size;
  const CGFloat width =
      std::max(contentSize.width + 2 * kFramePadding, minWidth_);
  const CGFloat height =
      contentSize.height + 2 * views::kRelatedControlVerticalSpacing;
  [self.view setFrameSize:NSMakeSize(width, height)];

  // Position the content view with some padding in the center of the view.
  [contentView_
      setFrameOrigin:NSMakePoint(kFramePadding,
                                 views::kRelatedControlVerticalSpacing)];

  // Add the borders, which go along the entire view.
  CGColorRef borderColor =
      gfx::CGColorCreateFromSkColor(ui::NativeTheme::instance()->GetSystemColor(
          ui::NativeTheme::kColorId_EnabledMenuButtonBorderColor));

  // Mac views don't have backing layers by default.
  base::scoped_nsobject<CALayer> rootLayer([[CALayer alloc] init]);
  [rootLayer setFrame:NSRectToCGRect(self.view.frame)];
  [self.view setLayer:rootLayer];
  [self.view setWantsLayer:YES];

  // The top border is only present for the first item.
  if (position_ == password_manager::ui::FIRST_ITEM) {
    base::scoped_nsobject<CALayer> topBorder([[CALayer alloc] init]);
    [topBorder setBackgroundColor:borderColor];
    [topBorder
        setFrame:CGRectMake(0, height - kBorderWidth, width, kBorderWidth)];
    [self.view.layer addSublayer:topBorder];
  }

  // The bottom border is always present.
  base::scoped_nsobject<CALayer> bottomBorder([[CALayer alloc] init]);
  [bottomBorder setBackgroundColor:borderColor];
  [bottomBorder setFrame:CGRectMake(0, 0, width, kBorderWidth)];
  [self.view.layer addSublayer:bottomBorder];
}

@end

@implementation ManagePasswordItemViewController (Testing)

- (ManagePasswordItemState)state {
  return state_;
}

- (NSView*)contentView {
  return contentView_.get();
}

@end
