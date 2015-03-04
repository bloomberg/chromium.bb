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
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace {
const CGFloat kHorizontalPaddingBetweenAvatarAndLabels = 10.0f;
const CGFloat kVerticalPaddingBetweenLabels = 2.0f;
}  // namespace

@interface CredentialItemView()
@property(nonatomic, readonly) NSTextField* nameLabel;
@property(nonatomic, readonly) NSTextField* usernameLabel;
@property(nonatomic, readonly) NSImageView* avatarView;
@end

@implementation CredentialItemView

@synthesize nameLabel = nameLabel_;
@synthesize usernameLabel = usernameLabel_;
@synthesize avatarView = avatarView_;
@synthesize passwordForm = passwordForm_;
@synthesize credentialType = credentialType_;

- (id)initWithPasswordForm:(const autofill::PasswordForm&)passwordForm
            credentialType:(password_manager::CredentialType)credentialType
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

    if (!passwordForm_.display_name.empty()) {
      nameLabel_ = [[[NSTextField alloc] initWithFrame:NSZeroRect] autorelease];
      [self addSubview:nameLabel_];
      [nameLabel_ setBezeled:NO];
      [nameLabel_ setDrawsBackground:NO];
      [nameLabel_ setEditable:NO];
      [nameLabel_ setSelectable:NO];
      [nameLabel_
          setStringValue:base::SysUTF16ToNSString(passwordForm_.display_name)];
      [nameLabel_ setAlignment:base::i18n::IsRTL() ? NSRightTextAlignment
                                                   : NSLeftTextAlignment];
      [nameLabel_ sizeToFit];
    }

    usernameLabel_ =
        [[[NSTextField alloc] initWithFrame:NSZeroRect] autorelease];
    [self addSubview:usernameLabel_];
    [usernameLabel_ setBezeled:NO];
    [usernameLabel_ setDrawsBackground:NO];
    [usernameLabel_ setEditable:NO];
    [usernameLabel_ setSelectable:NO];
    [usernameLabel_
        setStringValue:base::SysUTF16ToNSString(passwordForm_.username_value)];
    [usernameLabel_ setAlignment:base::i18n::IsRTL() ? NSRightTextAlignment
                                                     : NSLeftTextAlignment];
    [usernameLabel_ sizeToFit];

    // Compute the heights and widths of everything, as the layout depends on
    // these measurements.
    const CGFloat labelsHeight = NSHeight([nameLabel_ frame]) +
                                 NSHeight([usernameLabel_ frame]) +
                                 kVerticalPaddingBetweenLabels;
    const CGFloat height = std::max(labelsHeight, CGFloat(kAvatarImageSize));
    const CGFloat width =
        kAvatarImageSize + kHorizontalPaddingBetweenAvatarAndLabels +
        std::max(NSWidth([nameLabel_ frame]), NSWidth([usernameLabel_ frame]));
    self.frame = NSMakeRect(0, 0, width, height);

    // Lay out the views (RTL reverses the order horizontally).

    const CGFloat avatarX = base::i18n::IsRTL() ? width - kAvatarImageSize : 0;
    const CGFloat avatarY =
        (kAvatarImageSize > height) ? 0 : (height - kAvatarImageSize) / 2.0f;
    [avatarView_ setFrame:NSMakeRect(avatarX, avatarY, kAvatarImageSize,
                                     kAvatarImageSize)];

    const CGFloat usernameX =
        base::i18n::IsRTL()
            ? NSMinX([avatarView_ frame]) -
                  kHorizontalPaddingBetweenAvatarAndLabels -
                  NSWidth([usernameLabel_ frame])
            : NSMaxX([avatarView_ frame]) +
                  kHorizontalPaddingBetweenAvatarAndLabels;
    const CGFloat usernameLabelY =
        (labelsHeight > height) ? 0 : (height - labelsHeight) / 2.0f;
    NSRect usernameFrame = [usernameLabel_ frame];
    usernameFrame.origin = NSMakePoint(usernameX, usernameLabelY);
    [usernameLabel_ setFrame:usernameFrame];

    const CGFloat nameX = base::i18n::IsRTL()
                              ? NSMinX([avatarView_ frame]) -
                                    kHorizontalPaddingBetweenAvatarAndLabels -
                                    NSWidth([nameLabel_ frame])
                              : NSMaxX([avatarView_ frame]) +
                                    kHorizontalPaddingBetweenAvatarAndLabels;
    const CGFloat nameLabelY =
        NSMaxY(usernameFrame) + kVerticalPaddingBetweenLabels;
    NSRect nameFrame = [nameLabel_ frame];
    nameFrame.origin = NSMakePoint(nameX, nameLabelY);
    [nameLabel_ setFrame:nameFrame];

    // Use a default avatar and fetch the custom one, if it exists.
    [self updateAvatar:[[self class] defaultAvatar]];
    if (passwordForm_.avatar_url.is_valid())
      [delegate_ fetchAvatar:passwordForm_.avatar_url forView:self];

    // When resizing, stick to the left (resp. right for RTL) edge.
    const NSUInteger autoresizingMask =
        (base::i18n::IsRTL() ? NSViewMinXMargin : NSViewMaxXMargin);
    [avatarView_ setAutoresizingMask:autoresizingMask];
    [usernameLabel_ setAutoresizingMask:autoresizingMask];
    [nameLabel_ setAutoresizingMask:autoresizingMask];
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

@end
