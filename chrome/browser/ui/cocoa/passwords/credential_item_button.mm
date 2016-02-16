// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/credential_item_button.h"

#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {
const CGFloat kFocusRingLineWidth = 2;
const CGFloat kHorizontalPaddingBetweenAvatarAndLabel = 10;
}  // namespace

// Custom button cell that adds a left padding before the avatar, and a custom
// spacing between the avatar and label.
@interface CredentialItemButtonCell : NSButtonCell {
 @private
  // Padding added to the left margin of the button.
  int marginSpacing_;
  // Spacing between the cell image and title.
  int imageTitleSpacing_;
}

- (id)initWithMarginSpacing:(int)leftMarginSpacing
          imageTitleSpacing:(int)imageTitleSpacing;
@end

@implementation CredentialItemButtonCell
- (id)initWithMarginSpacing:(int)marginSpacing
          imageTitleSpacing:(int)imageTitleSpacing {
  if ((self = [super init])) {
    marginSpacing_ = marginSpacing;
    imageTitleSpacing_ = imageTitleSpacing;
  }
  return self;
}

- (NSRect)drawTitle:(NSAttributedString*)title
          withFrame:(NSRect)frame
             inView:(NSView*)controlView {
  // The title frame origin isn't aware of the left margin spacing added
  // in -drawImage, so it must be added when drawing the title.
  NSRect marginRect;
  NSDivideRect(frame, &marginRect, &frame, marginSpacing_, NSMinXEdge);
  NSDivideRect(frame, &marginRect, &frame, imageTitleSpacing_, NSMinXEdge);
  NSDivideRect(frame, &marginRect, &frame, marginSpacing_, NSMaxXEdge);

  return [super drawTitle:title withFrame:frame inView:controlView];
}

- (void)drawImage:(NSImage*)image
        withFrame:(NSRect)frame
           inView:(NSView*)controlView {
  frame.origin.x = marginSpacing_;
  gfx::ScopedNSGraphicsContextSaveGState scopedGState;
  NSBezierPath* path = [NSBezierPath bezierPathWithOvalInRect:frame];
  [path addClip];
  [super drawImage:image withFrame:frame inView:controlView];
}

- (NSSize)cellSize {
  NSSize buttonSize = [super cellSize];
  buttonSize.width += imageTitleSpacing_;
  buttonSize.width += 2 * marginSpacing_;
  return buttonSize;
}

- (NSFocusRingType)focusRingType {
  // This is taken care of by the custom drawing code.
  return NSFocusRingTypeNone;
}

- (void)drawWithFrame:(NSRect)frame inView:(NSView*)controlView {
  [super drawInteriorWithFrame:frame inView:controlView];

  // Focus ring.
  if ([self showsFirstResponder]) {
    NSRect focusRingRect =
        NSInsetRect(frame, kFocusRingLineWidth, kFocusRingLineWidth);
    // TODO(vasilii): When we are targetting 10.7, we should change this to use
    // -drawFocusRingMaskWithFrame instead.
    [[[NSColor keyboardFocusIndicatorColor] colorWithAlphaComponent:1] set];
    NSBezierPath* path = [NSBezierPath bezierPathWithRect:focusRingRect];
    [path setLineWidth:kFocusRingLineWidth];
    [path stroke];
  }
}

@end

@interface CredentialItemButton () {
  base::scoped_nsobject<NSColor> backgroundColor_;
  base::scoped_nsobject<NSColor> hoverColor_;
}
@end

@implementation CredentialItemButton
@synthesize passwordForm = passwordForm_;
@synthesize credentialType = credentialType_;

- (id)initWithFrame:(NSRect)frameRect
    backgroundColor:(NSColor*)backgroundColor
         hoverColor:(NSColor*)hoverColor {
  if ((self = [super initWithFrame:frameRect])) {
    backgroundColor_.reset([backgroundColor retain]);
    hoverColor_.reset([hoverColor retain]);

    base::scoped_nsobject<CredentialItemButtonCell> cell(
        [[CredentialItemButtonCell alloc]
            initWithMarginSpacing:kFramePadding
                imageTitleSpacing:kHorizontalPaddingBetweenAvatarAndLabel]);
    [cell setLineBreakMode:NSLineBreakByTruncatingTail];
    [cell setHighlightsBy:NSChangeGrayCellMask];
    [self setCell:cell.get()];

    [self setBordered:NO];
    [self setFont:ResourceBundle::GetSharedInstance()
                      .GetFontList(ResourceBundle::SmallFont)
                      .GetPrimaryFont()
                      .GetNativeFont()];
    [self setButtonType:NSMomentaryLightButton];
    [self setImagePosition:NSImageLeft];
    [self setAlignment:NSLeftTextAlignment];
  }
  return self;
}

+ (NSImage*)defaultAvatar {
  return gfx::NSImageFromImageSkia(ScaleImageForAccountAvatar(
      *ResourceBundle::GetSharedInstance()
           .GetImageNamed(IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE)
           .ToImageSkia()));
}

- (void)setHoverState:(HoverState)state {
  [super setHoverState:state];
  bool isHighlighted = ([self hoverState] != kHoverStateNone);

  NSColor* backgroundColor = isHighlighted ? hoverColor_ : backgroundColor_;
  [[self cell] setBackgroundColor:backgroundColor];
}

- (BOOL)canBecomeKeyView {
  return YES;
}

@end
