// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/credential_item_button.h"

#include "base/i18n/rtl.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_tooltip_controller.h"
#include "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/gfx/vector_icons_public.h"

namespace {
constexpr CGFloat kFocusRingInset = 3;
constexpr CGFloat kHorizontalPaddingBetweenAvatarAndLabel = 10;
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
  NSDivideRect(frame, &marginRect, &frame, imageTitleSpacing_,
               base::i18n::IsRTL() ? NSMaxXEdge : NSMinXEdge);
  NSDivideRect(frame, &marginRect, &frame, marginSpacing_, NSMaxXEdge);

  return [super drawTitle:title withFrame:frame inView:controlView];
}

- (void)drawImage:(NSImage*)image
        withFrame:(NSRect)frame
           inView:(NSView*)controlView {
  if (base::i18n::IsRTL())
    frame.origin.x -= marginSpacing_;
  else
    frame.origin.x += marginSpacing_;
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

- (void)drawFocusRingMaskWithFrame:(NSRect)cellFrame
                            inView:(NSView *)controlView {
  NSRect focusRingRect =
      NSInsetRect(cellFrame, kFocusRingInset, kFocusRingInset);

  [[NSBezierPath bezierPathWithRoundedRect:focusRingRect
                                   xRadius:2
                                   yRadius:2] fill];
}

@end

@interface CredentialItemButton () {
  base::scoped_nsobject<NSColor> backgroundColor_;
  base::scoped_nsobject<NSColor> hoverColor_;
  base::scoped_nsobject<AutofillTooltipController> iconController_;
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
    [self setImagePosition:base::i18n::IsRTL() ? NSImageRight : NSImageLeft];
    [self setAlignment:NSNaturalTextAlignment];
  }
  return self;
}

- (NSView*)addInfoIcon:(NSString*)tooltip {
  DCHECK(!iconController_);
  iconController_.reset([[AutofillTooltipController alloc]
      initWithArrowLocation:info_bubble::kTopTrailing]);
  NSImage* image = gfx::NSImageFromImageSkia(gfx::CreateVectorIcon(
      gfx::VectorIconId::INFO_OUTLINE, gfx::kChromeIconGrey));
  [iconController_ setImage:image];
  [iconController_ setMessage:tooltip];
  [self addSubview:[iconController_ view]];
  return [iconController_ view];
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
