// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/avatar_button_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "ui/base/cocoa/appkit_utils.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace {

const CGFloat kButtonPadding = 12;
const CGFloat kButtonDefaultPadding = 5;
const CGFloat kButtonHeight = 27;
const CGFloat kButtonTitleImageSpacing = 10;

const ui::NinePartImageIds kNormalBorderImageIds =
    IMAGE_GRID(IDR_AVATAR_MAC_BUTTON_NORMAL);
const ui::NinePartImageIds kHoverBorderImageIds =
    IMAGE_GRID(IDR_AVATAR_MAC_BUTTON_HOVER);
const ui::NinePartImageIds kPressedBorderImageIds =
    IMAGE_GRID(IDR_AVATAR_MAC_BUTTON_PRESSED);
const ui::NinePartImageIds kThemedBorderImageIds =
    IMAGE_GRID(IDR_AVATAR_THEMED_MAC_BUTTON_NORMAL);

NSImage* GetImageFromResourceID(int resourceId) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resourceId).ToNSImage();
}

}  // namespace

// Button cell with a custom border given by a set of nine-patch image grids.
@interface CustomThemeButtonCell : NSButtonCell {
 @private
   BOOL isThemedWindow_;
   base::scoped_nsobject<NSImage> authenticationErrorImage_;
}
- (void)setIsThemedWindow:(BOOL)isThemedWindow;
- (void)setHasError:(BOOL)hasError withTitle:(NSString*)title;

@end

@implementation CustomThemeButtonCell
- (id)initWithThemedWindow:(BOOL)isThemedWindow {
  if ((self = [super init])) {
    isThemedWindow_ = isThemedWindow;
    authenticationErrorImage_.reset();
  }
  return self;
}

- (NSSize)cellSize {
  NSSize buttonSize = [super cellSize];
  CGFloat errorWidth = [authenticationErrorImage_ size].width;
  buttonSize.width += 2 * (kButtonPadding - kButtonDefaultPadding) + errorWidth;
  buttonSize.height = kButtonHeight;
  return buttonSize;
}

- (NSRect)drawTitle:(NSAttributedString*)title
          withFrame:(NSRect)frame
             inView:(NSView*)controlView {
  frame.origin.x = kButtonPadding;

  // If there's an auth error, draw a warning icon before the cell image.
  if (authenticationErrorImage_) {
    NSSize imageSize = [authenticationErrorImage_ size];
    NSRect rect = NSMakeRect(
        frame.size.width - imageSize.width,
        (kButtonHeight - imageSize.height) / 2,
        imageSize.width,
        imageSize.height);
    [authenticationErrorImage_ drawInRect:rect
                       fromRect:NSZeroRect
                      operation:NSCompositeSourceOver
                       fraction:1.0
                 respectFlipped:YES
                          hints:nil];
    // Padding between the title and the error image.
    frame.size.width -= kButtonTitleImageSpacing;
  }

  // Padding between the title (or error image, if it exists) and the
  // button's drop down image.
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
  if (isThemedWindow_)
    imageIds = kThemedBorderImageIds;

  if (hoverState == kHoverStateMouseDown)
    imageIds = kPressedBorderImageIds;
  else if (hoverState == kHoverStateMouseOver)
    imageIds = kHoverBorderImageIds;
  ui::DrawNinePartImage(frame, imageIds, NSCompositeSourceOver, 1.0, true);
}

- (void)setIsThemedWindow:(BOOL)isThemedWindow {
  isThemedWindow_ = isThemedWindow;
}

- (void)setHasError:(BOOL)hasError withTitle:(NSString*)title {
  if (hasError) {
    authenticationErrorImage_.reset(
        [ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_ICON_PROFILES_AVATAR_BUTTON_ERROR).ToNSImage() retain]);
    [self accessibilitySetOverrideValue:l10n_util::GetNSStringF(
        IDS_PROFILES_ACCOUNT_BUTTON_AUTH_ERROR_ACCESSIBLE_NAME,
        base::SysNSStringToUTF16(title))
                           forAttribute:NSAccessibilityTitleAttribute];
  } else {
    authenticationErrorImage_.reset();
    [self accessibilitySetOverrideValue:title
                          forAttribute:NSAccessibilityTitleAttribute];
  }
}

@end

@interface AvatarButtonController (Private)
- (void)updateAvatarButtonAndLayoutParent:(BOOL)layoutParent;
- (void)updateErrorStatus:(BOOL)hasError;
- (void)dealloc;
- (void)themeDidChangeNotification:(NSNotification*)aNotification;
@end

@implementation AvatarButtonController

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super initWithBrowser:browser])) {
    ThemeService* themeService =
        ThemeServiceFactory::GetForProfile(browser->profile());
    isThemedWindow_ = !themeService->UsingSystemTheme();

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
        [[CustomThemeButtonCell alloc] initWithThemedWindow:isThemedWindow_]);
    SigninErrorController* errorController =
        profiles::GetSigninErrorController(browser->profile());

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
    if (errorController)
      [cell setHasError:errorController->HasError() withTitle:[button_ title]];

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(themeDidChangeNotification:)
                   name:kBrowserThemeDidChangeNotification
                 object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)themeDidChangeNotification:(NSNotification*)aNotification {
  // Redraw the button if the window has switched between themed and native.
  ThemeService* themeService =
      ThemeServiceFactory::GetForProfile(browser_->profile());
  BOOL updatedIsThemedWindow = !themeService->UsingSystemTheme();
  if (isThemedWindow_ != updatedIsThemedWindow) {
    isThemedWindow_ = updatedIsThemedWindow;
    [[button_ cell] setIsThemedWindow:isThemedWindow_];
    [self updateAvatarButtonAndLayoutParent:YES];
  }
}

- (void)updateAvatarButtonAndLayoutParent:(BOOL)layoutParent {
  // The button text has a black foreground and a white drop shadow for regular
  // windows, and a light text with a dark drop shadow for guest windows
  // which are themed with a dark background.
  base::scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
  [shadow setShadowOffset:NSMakeSize(0, -1)];
  [shadow setShadowBlurRadius:0];

  NSColor* foregroundColor;
  if (browser_->profile()->IsGuestSession()) {
    foregroundColor = [NSColor colorWithCalibratedWhite:1.0 alpha:0.9];
    [shadow setShadowColor:[NSColor colorWithCalibratedWhite:0.0 alpha:0.4]];
  } else if (!isThemedWindow_) {
    foregroundColor = [NSColor blackColor];
    [shadow setShadowColor:[NSColor colorWithCalibratedWhite:1.0 alpha:0.7]];
  } else {
    foregroundColor = [NSColor blackColor];
    [shadow setShadowColor:[NSColor colorWithCalibratedWhite:1.0 alpha:0.4]];
  }

  NSString* buttonTitle = base::SysUTF16ToNSString(
      profiles::GetAvatarButtonTextForProfile(browser_->profile()));

  base::scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setAlignment:NSLeftTextAlignment];

  base::scoped_nsobject<NSAttributedString> attributedTitle(
      [[NSAttributedString alloc]
          initWithString:buttonTitle
              attributes:@{ NSShadowAttributeName : shadow.get(),
                            NSForegroundColorAttributeName : foregroundColor,
                            NSParagraphStyleAttributeName : paragraphStyle }]);
  [button_ setAttributedTitle:attributedTitle];
  [button_ sizeToFit];

  if (layoutParent) {
    // Because the width of the button might have changed, the parent browser
    // frame needs to recalculate the button bounds and redraw it.
    [[BrowserWindowController
        browserWindowControllerForWindow:browser_->window()->GetNativeWindow()]
        layoutSubviews];
  }
}

- (void)updateErrorStatus:(BOOL)hasError {
  [[button_ cell] setHasError:hasError withTitle:[button_ title]];
  [self updateAvatarButtonAndLayoutParent:YES];
}

@end
