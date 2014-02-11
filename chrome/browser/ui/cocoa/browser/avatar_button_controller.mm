// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/avatar_button_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "ui/base/cocoa/appkit_utils.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/text_elider.h"

namespace {

const CGFloat kButtonPadding = 12;
const CGFloat kButtonDefaultPadding = 5;
const CGFloat kButtonHeight = 27;
const CGFloat kButtonTitleImageSpacing = 10;
const CGFloat kMaxButtonWidth = 120;

const ui::NinePartImageIds kNormalBorderImageIds =
    IMAGE_GRID(IDR_AVATAR_MAC_BUTTON_NORMAL);
const ui::NinePartImageIds kHoverBorderImageIds =
    IMAGE_GRID(IDR_AVATAR_MAC_BUTTON_HOVER);
const ui::NinePartImageIds kPressedBorderImageIds =
    IMAGE_GRID(IDR_AVATAR_MAC_BUTTON_PRESSED);

NSImage* GetImageFromResourceID(int resourceId) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resourceId).ToNSImage();
}

}  // namespace

// Button cell with a custom border given by a set of nine-patch image grids.
@interface CustomThemeButtonCell : NSButtonCell
@end

@implementation CustomThemeButtonCell
- (NSSize)cellSize {
  NSSize buttonSize = [super cellSize];
  buttonSize.width += 2 * kButtonPadding - 2 * kButtonDefaultPadding;
  buttonSize.height = kButtonHeight;
  return buttonSize;
}

- (NSRect)drawTitle:(NSAttributedString*)title
          withFrame:(NSRect)frame
             inView:(NSView*)controlView {
  frame.origin.x = kButtonPadding;
  // Ensure there's always a padding between the text and the image.
  frame.size.width -= kButtonTitleImageSpacing;
  return [super drawTitle:title withFrame:frame inView:controlView];
}

- (void)drawImage:(NSImage*)image
        withFrame:(NSRect)frame
           inView:(NSView*)controlView {
  // For the x-offset, we need to undo the default padding and apply the
  // new one. For the y-offset, increasing the button height means we need
  // to move the image a little down to align it nicely with the text; this
  // was chosen by visual inspection.
  frame = NSOffsetRect(frame, kButtonDefaultPadding - kButtonPadding, 2);
  [super drawImage:image withFrame:frame inView:controlView];
}

- (void)drawBezelWithFrame:(NSRect)frame
                    inView:(NSView*)controlView {
  HoverState hoverState =
      [base::mac::ObjCCastStrict<HoverImageButton>(controlView) hoverState];
  ui::NinePartImageIds imageIds = kNormalBorderImageIds;

  if (hoverState == kHoverStateMouseDown)
    imageIds = kPressedBorderImageIds;
  else if (hoverState == kHoverStateMouseOver)
    imageIds = kHoverBorderImageIds;
  ui::DrawNinePartImage(frame, imageIds, NSCompositeSourceOver, 1.0, true);
}
@end

@interface AvatarButtonController (Private)
- (void)updateAvatarButtonAndLayoutParent:(BOOL)layoutParent;
@end

@implementation AvatarButtonController

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super initWithBrowser:browser])) {
    HoverImageButton* hoverButton =
        [[HoverImageButton alloc] initWithFrame:NSZeroRect];
    [hoverButton setDefaultImage:GetImageFromResourceID(
        IDR_AVATAR_MAC_BUTTON_DROPARROW)];
    [hoverButton setHoverImage:GetImageFromResourceID(
        IDR_AVATAR_MAC_BUTTON_DROPARROW_HOVER)];
    [hoverButton setPressedImage:GetImageFromResourceID(
        IDR_AVATAR_MAC_BUTTON_DROPARROW_PRESSED)];

    button_.reset(hoverButton);
    base::scoped_nsobject<CustomThemeButtonCell> cell(
        [[CustomThemeButtonCell alloc] init]);
    [button_ setCell:cell.get()];
    [self setView:button_];

    [button_ setBezelStyle:NSShadowlessSquareBezelStyle];
    [button_ setButtonType:NSMomentaryChangeButton];
    [button_ setBordered:YES];
    // This is a workaround for an issue in the HoverImageButton where the
    // button is initially sized incorrectly unless a default image is provided.
    [button_ setImage:GetImageFromResourceID(IDR_AVATAR_MAC_BUTTON_DROPARROW)];
    [button_ setImagePosition:NSImageRight];
    [button_ setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
    [button_ setTarget:self];
    [button_ setAction:@selector(buttonClicked:)];

    [self updateAvatarButtonAndLayoutParent:NO];
  }
  return self;
}

- (void)updateAvatarButtonAndLayoutParent:(BOOL)layoutParent {
  // The button text has a white drop shadow.
  base::scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
  [shadow setShadowOffset:NSMakeSize(0, -1)];
  [shadow setShadowBlurRadius:0];
  [shadow setShadowColor:[NSColor whiteColor]];

  base::scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setLineBreakMode:NSLineBreakByTruncatingTail];
  [paragraphStyle setAlignment:NSLeftTextAlignment];

  NSString* buttonTitle = base::SysUTF16ToNSString(
      profiles::GetActiveProfileDisplayName(browser_));

  base::scoped_nsobject<NSAttributedString> attributedTitle(
      [[NSAttributedString alloc]
          initWithString:buttonTitle
              attributes:@{ NSShadowAttributeName : shadow.get(),
                            NSParagraphStyleAttributeName : paragraphStyle }]);
  [button_ setAttributedTitle:attributedTitle];
  [button_ sizeToFit];

  // Truncate the title if needed.
  if (NSWidth([button_ bounds]) > kMaxButtonWidth)
    [button_ setFrameSize:NSMakeSize(kMaxButtonWidth, kButtonHeight)];

  if (layoutParent) {
    // Because the width of the button might have changed, the parent browser
    // frame needs to recalculate the button bounds and redraw it.
    [[BrowserWindowController
        browserWindowControllerForWindow:browser_->window()->GetNativeWindow()]
        layoutSubviews];
  }
}

@end
