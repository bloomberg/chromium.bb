// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/avatar_button_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "grit/theme_resources.h"
#import "ui/base/cocoa/appkit_utils.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace {

// NSButtons have a default padding of 5px. This button should have a padding
// of 8px.
const CGFloat kButtonExtraPadding = 8 - 5;
const CGFloat kButtonHeight = 28;

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
   BOOL hasError_;
}
- (void)setIsThemedWindow:(BOOL)isThemedWindow;
- (void)setHasError:(BOOL)hasError withTitle:(NSString*)title;

@end

@implementation CustomThemeButtonCell
- (id)initWithThemedWindow:(BOOL)isThemedWindow {
  if ((self = [super init])) {
    isThemedWindow_ = isThemedWindow;
    hasError_ = false;
  }
  return self;
}

- (NSSize)cellSize {
  NSSize buttonSize = [super cellSize];

  // An image and no error means we are drawing the generic button, which
  // is square. Otherwise, we are displaying the profile's name and an
  // optional authentication error icon.
  if ([self image] && !hasError_) {
    buttonSize.width = kButtonHeight;
  } else {
    buttonSize.width += 2 * kButtonExtraPadding;
  }
  buttonSize.height = kButtonHeight;
  return buttonSize;
}

- (void)drawInteriorWithFrame:(NSRect)frame inView:(NSView*)controlView {
  NSRect frameAfterPadding = NSInsetRect(frame, kButtonExtraPadding, 0);
  [super drawInteriorWithFrame:frameAfterPadding inView:controlView];
}

- (void)drawImage:(NSImage*)image
        withFrame:(NSRect)frame
           inView:(NSView*)controlView {
  // The image used in the generic button case needs to be shifted down
  // slightly to be centered correctly.
  // TODO(noms): When the assets are fixed, remove this latter offset.
  if (!hasError_)
    frame = NSOffsetRect(frame, 0, 1);
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
  hasError_ = hasError;
  if (hasError) {
    [self accessibilitySetOverrideValue:l10n_util::GetNSStringF(
        IDS_PROFILES_ACCOUNT_BUTTON_AUTH_ERROR_ACCESSIBLE_NAME,
        base::SysNSStringToUTF16(title))
                           forAttribute:NSAccessibilityTitleAttribute];
  } else {
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
    button_.reset(hoverButton);
    base::scoped_nsobject<CustomThemeButtonCell> cell(
        [[CustomThemeButtonCell alloc] initWithThemedWindow:isThemedWindow_]);
    [button_ setCell:cell.get()];

    // Check if the account already has an authentication error.
    SigninErrorController* errorController =
        profiles::GetSigninErrorController(browser->profile());
    hasError_ = errorController && errorController->HasError();
    [cell setHasError:hasError_ withTitle:nil];

    [button_ setWantsLayer:YES];
    [self setView:button_];

    [button_ setBezelStyle:NSShadowlessSquareBezelStyle];
    [button_ setButtonType:NSMomentaryChangeButton];
    [button_ setBordered:YES];

    [button_ setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
    [button_ setTarget:self];
    [button_ setAction:@selector(buttonClicked:)];

    [self updateAvatarButtonAndLayoutParent:NO];

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

  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  // If there is a single local profile, then use the generic avatar button
  // instead of the profile name. Never use the generic button if the active
  // profile is Guest.
  bool useGenericButton = (!browser_->profile()->IsGuestSession() &&
                           cache.GetNumberOfProfiles() == 1 &&
                           cache.GetUserNameOfProfileAtIndex(0).empty());


  NSString* buttonTitle = base::SysUTF16ToNSString(useGenericButton ?
      base::string16() :
      profiles::GetAvatarButtonTextForProfile(browser_->profile()));

  HoverImageButton* button =
      base::mac::ObjCCastStrict<HoverImageButton>(button_);
  if (useGenericButton) {
    [button setDefaultImage:GetImageFromResourceID(
        IDR_AVATAR_MAC_BUTTON_AVATAR)];
    [button setHoverImage:GetImageFromResourceID(
        IDR_AVATAR_MAC_BUTTON_AVATAR_HOVER)];
    [button setPressedImage:GetImageFromResourceID(
        IDR_AVATAR_MAC_BUTTON_AVATAR_PRESSED)];
    // This is a workaround for an issue in the HoverImageButton where the
    // button is initially sized incorrectly unless a default image is provided.
    // See crbug.com/298501.
    [button setImage:GetImageFromResourceID(IDR_AVATAR_MAC_BUTTON_AVATAR)];
    [button setImagePosition:NSImageOnly];
  } else if (hasError_) {
    [button setDefaultImage:GetImageFromResourceID(
        IDR_ICON_PROFILES_AVATAR_BUTTON_ERROR)];
    [button setHoverImage:nil];
    [button setPressedImage:nil];
    [button setImage:GetImageFromResourceID(
        IDR_ICON_PROFILES_AVATAR_BUTTON_ERROR)];
    [button setImagePosition:NSImageRight];
  } else {
    [button setDefaultImage:nil];
    [button setHoverImage:nil];
    [button setPressedImage:nil];
    [button setImagePosition:NSNoImage];
  }

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
  hasError_ = hasError;
  [[button_ cell] setHasError:hasError withTitle:[button_ title]];
  [self updateAvatarButtonAndLayoutParent:YES];
}

@end
