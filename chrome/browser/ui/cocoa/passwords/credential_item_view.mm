// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/credential_item_view.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace {
const CGFloat kHorizontalPaddingBetweenAvatarAndLabels = 10.0f;
const CGFloat kVerticalPaddingBetweenLabels = 2.0f;
}  // namespace

@interface CredentialItemView()
+ (NSString*)upperLabelTextForForm:(const autofill::PasswordForm&)passwordForm
                             style:(password_manager_mac::CredentialItemStyle)
                                       style;
+ (NSString*)lowerLabelTextForForm:
        (const autofill::PasswordForm&)passwordForm;
+ (NSTextField*)labelWithText:(NSString*)title;
@property(nonatomic, readonly) NSTextField* upperLabel;
@property(nonatomic, readonly) NSTextField* lowerLabel;
@property(nonatomic, readonly) NSImageView* avatarView;
@end

@implementation CredentialItemView

@synthesize upperLabel = upperLabel_;
@synthesize lowerLabel = lowerLabel_;
@synthesize avatarView = avatarView_;
@synthesize passwordForm = passwordForm_;
@synthesize credentialType = credentialType_;

- (id)initWithPasswordForm:(const autofill::PasswordForm&)passwordForm
            credentialType:(password_manager::CredentialType)credentialType
                     style:(password_manager_mac::CredentialItemStyle)style
                  delegate:(id<CredentialItemDelegate>)delegate {
  if ((self = [super init])) {
    passwordForm_ = passwordForm;
    credentialType_ = credentialType;
    delegate_ = delegate;

    // -----------------------------------------------
    // |      | John Q. Facebooker                   |
    // | icon | john@somewhere.com                   |
    // -----------------------------------------------

    // Create the views.

    avatarView_ = [[[NSImageView alloc] initWithFrame:NSZeroRect] autorelease];
    [avatarView_ setWantsLayer:YES];
    [[avatarView_ layer] setCornerRadius:kAvatarImageSize / 2.0f];
    [[avatarView_ layer] setMasksToBounds:YES];
    [self addSubview:avatarView_];

    NSString* upperLabelText =
        [[self class] upperLabelTextForForm:passwordForm_ style:style];
    upperLabel_ = [[self class] labelWithText:upperLabelText];
    [self addSubview:upperLabel_];

    NSString* lowerLabelText =
        [[self class] lowerLabelTextForForm:passwordForm_];
    if (lowerLabelText) {
      lowerLabel_ = [[self class] labelWithText:lowerLabelText];
      [self addSubview:lowerLabel_];
    }

    // Compute the heights and widths of everything, as the layout depends on
    // these measurements.
    const CGFloat labelsHeight = NSHeight([upperLabel_ frame]) +
                                 NSHeight([lowerLabel_ frame]) +
                                 kVerticalPaddingBetweenLabels;
    const CGFloat height = std::max(labelsHeight, CGFloat(kAvatarImageSize));
    const CGFloat width =
        kAvatarImageSize + kHorizontalPaddingBetweenAvatarAndLabels +
        std::max(NSWidth([upperLabel_ frame]), NSWidth([lowerLabel_ frame]));
    self.frame = NSMakeRect(0, 0, width, height);

    // Lay out the views (RTL reverses the order horizontally).

    const CGFloat avatarX = base::i18n::IsRTL() ? width - kAvatarImageSize : 0;
    const CGFloat avatarY =
        (kAvatarImageSize > height) ? 0 : (height - kAvatarImageSize) / 2.0f;
    [avatarView_ setFrame:NSMakeRect(avatarX, avatarY, kAvatarImageSize,
                                     kAvatarImageSize)];

    const CGFloat lowerX =
        base::i18n::IsRTL()
            ? NSMinX([avatarView_ frame]) -
                  kHorizontalPaddingBetweenAvatarAndLabels -
                  NSWidth([lowerLabel_ frame])
            : NSMaxX([avatarView_ frame]) +
                  kHorizontalPaddingBetweenAvatarAndLabels;
    const CGFloat lowerLabelY =
        (labelsHeight > height) ? 0 : (height - labelsHeight) / 2.0f;
    NSRect lowerFrame = [lowerLabel_ frame];
    lowerFrame.origin = NSMakePoint(lowerX, lowerLabelY);
    [lowerLabel_ setFrame:lowerFrame];

    const CGFloat upperX = base::i18n::IsRTL()
                              ? NSMinX([avatarView_ frame]) -
                                    kHorizontalPaddingBetweenAvatarAndLabels -
                                    NSWidth([upperLabel_ frame])
                              : NSMaxX([avatarView_ frame]) +
                                    kHorizontalPaddingBetweenAvatarAndLabels;
    const CGFloat upperLabelY =
        NSMaxY(lowerFrame) + kVerticalPaddingBetweenLabels;
    NSRect upperFrame = [upperLabel_ frame];
    upperFrame.origin = NSMakePoint(upperX, upperLabelY);
    [upperLabel_ setFrame:upperFrame];

    // Use a default avatar and fetch the custom one, if it exists.
    [self updateAvatar:[[self class] defaultAvatar]];
    if (passwordForm_.icon_url.is_valid())
      [delegate_ fetchAvatar:passwordForm_.icon_url forView:self];

    // When resizing, stick to the left (resp. right for RTL) edge.
    const NSUInteger autoresizingMask =
        (base::i18n::IsRTL() ? NSViewMinXMargin : NSViewMaxXMargin);
    [avatarView_ setAutoresizingMask:autoresizingMask];
    [lowerLabel_ setAutoresizingMask:autoresizingMask];
    [upperLabel_ setAutoresizingMask:autoresizingMask];
    [self setAutoresizingMask:NSViewWidthSizable];
  }

  return self;
}

- (void)updateAvatar:(NSImage*)avatar {
  [avatarView_ setImage:avatar];
}

+ (NSImage*)defaultAvatar {
  return gfx::NSImageFromImageSkia(ScaleImageForAccountAvatar(
      *ResourceBundle::GetSharedInstance()
           .GetImageNamed(IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE)
           .ToImageSkia()));
}

+ (NSString*)upperLabelTextForForm:(const autofill::PasswordForm&)passwordForm
                             style:(password_manager_mac::CredentialItemStyle)
                                       style {
  base::string16 name = passwordForm.display_name.empty()
                            ? passwordForm.username_value
                            : passwordForm.display_name;
  switch (style) {
    case password_manager_mac::CredentialItemStyle::ACCOUNT_CHOOSER:
      return base::SysUTF16ToNSString(name);
    case password_manager_mac::CredentialItemStyle::AUTO_SIGNIN:
      return l10n_util::GetNSStringF(IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TITLE,
                                     name);
  }
  NOTREACHED();
  return nil;
}

+ (NSString*)lowerLabelTextForForm:
        (const autofill::PasswordForm&)passwordForm {
  return passwordForm.display_name.empty()
             ? nil
             : base::SysUTF16ToNSString(passwordForm.username_value);
}

+ (NSTextField*)labelWithText:(NSString*)title {
  NSTextField* label =
      [[[NSTextField alloc] initWithFrame:NSZeroRect] autorelease];
  [label setBezeled:NO];
  [label setDrawsBackground:NO];
  [label setEditable:NO];
  [label setSelectable:NO];
  [label setStringValue:title];
  [label setAlignment:base::i18n::IsRTL() ? NSRightTextAlignment
                                          : NSLeftTextAlignment];
  [label sizeToFit];
  return label;
}

@end
