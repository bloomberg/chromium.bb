// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/avatar_button_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/l10n_util.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_button.h"
#include "chrome/grit/generated_resources.h"
#import "chrome/browser/themes/theme_properties.h"
#include "chrome/grit/theme_resources.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/appkit_utils.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"

namespace {

const ui::NinePartImageIds kNormalBorderImageIds =
    IMAGE_GRID(IDR_AVATAR_NATIVE_BUTTON_NORMAL);
const ui::NinePartImageIds kHoverBorderImageIds =
    IMAGE_GRID(IDR_AVATAR_NATIVE_BUTTON_HOVER);
const ui::NinePartImageIds kPressedBorderImageIds =
    IMAGE_GRID(IDR_AVATAR_NATIVE_BUTTON_PRESSED);
const ui::NinePartImageIds kThemedBorderImageIds =
    IMAGE_GRID(IDR_AVATAR_THEMED_MAC_BUTTON_NORMAL);

NSImage* GetImageFromResourceID(int resourceId) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resourceId).ToNSImage();
}

const SkColor kMaterialButtonHoverColor = SkColorSetARGB(20, 0, 0, 0);
const SkColor kMaterialButtonPressedColor = SkColorSetARGB(31, 0, 0, 0);
const SkColor kMaterialAvatarIconColor = SkColorSetRGB(0x5a, 0x5a, 0x5a);

CGFloat ButtonHeight() {
  const CGFloat kButtonHeight = 28;
  const CGFloat kMaterialButtonHeight = 24;
  return ui::MaterialDesignController::IsModeMaterial() ? kMaterialButtonHeight
                                                        : kButtonHeight;
}

// NSButtons have a default padding of 5px. Non-MD buttons should have a
// padding of 8px. Meanwhile, MD buttons should have a padding of 6px.
CGFloat ButtonExtraPadding() {
  const CGFloat kDefaultPadding = 5;
  const CGFloat kButtonExtraPadding = 8 - kDefaultPadding;
  const CGFloat kMaterialButtonExtraPadding = 6 - kDefaultPadding;

  return ui::MaterialDesignController::IsModeMaterial()
             ? kMaterialButtonExtraPadding
             : kButtonExtraPadding;
}

// Extra padding for the MD signed out avatar button.
const CGFloat kMaterialSignedOutWidthPadding = 2;

// Kern value for the MD avatar button title.
const CGFloat kMaterialTitleKern = 0.25;

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
    buttonSize.width = ButtonHeight();
    if (ui::MaterialDesignController::IsModeMaterial())
      buttonSize.width += kMaterialSignedOutWidthPadding;
  } else {
    buttonSize.width += 2 * ButtonExtraPadding();
  }
  buttonSize.height = ButtonHeight();
  return buttonSize;
}

- (void)drawInteriorWithFrame:(NSRect)frame inView:(NSView*)controlView {
  NSRect frameAfterPadding = NSInsetRect(frame, ButtonExtraPadding(), 0);
  [super drawInteriorWithFrame:frameAfterPadding inView:controlView];
}

- (void)drawImage:(NSImage*)image
        withFrame:(NSRect)frame
           inView:(NSView*)controlView {
  // The image used in the generic button case as well as the material-designed
  // error icon both need to be shifted down slightly to be centered correctly.
  // TODO(noms): When the assets are fixed, remove this latter offset.
  if (!hasError_ || switches::IsMaterialDesignUserMenu())
    frame = NSOffsetRect(frame, 0, 1);
  [super drawImage:image withFrame:frame inView:controlView];
}

- (void)drawBezelWithFrame:(NSRect)frame
                    inView:(NSView*)controlView {
  HoverState hoverState =
      [base::mac::ObjCCastStrict<AvatarButton>(controlView) hoverState];

  if (ui::MaterialDesignController::IsModeMaterial()) {
    NSColor* backgroundColor = nil;
    if (hoverState == kHoverStateMouseDown) {
      backgroundColor = skia::SkColorToSRGBNSColor(kMaterialButtonPressedColor);
    } else if (hoverState == kHoverStateMouseOver) {
      backgroundColor = skia::SkColorToSRGBNSColor(kMaterialButtonHoverColor);
    }

    if (backgroundColor) {
      [backgroundColor set];
      NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:frame
                                                           xRadius:2.0f
                                                           yRadius:2.0f];
      [path fill];
    }
  } else {
    ui::NinePartImageIds imageIds = kNormalBorderImageIds;
    if (isThemedWindow_)
      imageIds = kThemedBorderImageIds;

    if (hoverState == kHoverStateMouseDown)
      imageIds = kPressedBorderImageIds;
    else if (hoverState == kHoverStateMouseOver)
      imageIds = kHoverBorderImageIds;
    ui::DrawNinePartImage(frame, imageIds, NSCompositeSourceOver, 1.0, true);
  }
}

- (void)drawFocusRingMaskWithFrame:(NSRect)frame inView:(NSView*)view {
  // Match the bezel's shape.
  [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(frame, 2, 2)
                                   xRadius:2
                                   yRadius:2] fill];
}

- (void)setIsThemedWindow:(BOOL)isThemedWindow {
  isThemedWindow_ = isThemedWindow;
}

- (void)setHasError:(BOOL)hasError withTitle:(NSString*)title {
  hasError_ = hasError;
  int messageId = hasError ?
      IDS_PROFILES_ACCOUNT_BUTTON_AUTH_ERROR_ACCESSIBLE_NAME :
      IDS_PROFILES_NEW_AVATAR_BUTTON_ACCESSIBLE_NAME;

  [self accessibilitySetOverrideValue:l10n_util::GetNSStringF(
      messageId, base::SysNSStringToUTF16(title))
                         forAttribute:NSAccessibilityTitleAttribute];
}

@end

@interface AvatarButtonController (Private)
- (void)updateAvatarButtonAndLayoutParent:(BOOL)layoutParent;
- (void)setErrorStatus:(BOOL)hasError;
- (void)dealloc;
- (void)themeDidChangeNotification:(NSNotification*)aNotification;
@end

@implementation AvatarButtonController

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super initWithBrowser:browser])) {
    ThemeService* themeService =
        ThemeServiceFactory::GetForProfile(browser->profile());
    isThemedWindow_ = !themeService->UsingSystemTheme();

    AvatarButton* avatarButton =
        [[AvatarButton alloc] initWithFrame:NSZeroRect];
    button_.reset(avatarButton);

    base::scoped_nsobject<NSButtonCell> cell(
        [[CustomThemeButtonCell alloc] initWithThemedWindow:isThemedWindow_]);

    [avatarButton setCell:cell.get()];

    [avatarButton setWantsLayer:YES];
    [self setView:avatarButton];

    [avatarButton setBezelStyle:NSShadowlessSquareBezelStyle];
    [avatarButton setButtonType:NSMomentaryChangeButton];
    if (switches::IsMaterialDesignUserMenu())
      [[avatarButton cell] setHighlightsBy:NSNoCellMask];
    [avatarButton setBordered:YES];

    if (cocoa_l10n_util::ShouldDoExperimentalRTLLayout())
      [avatarButton setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
    else
      [avatarButton setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
    [avatarButton setTarget:self];
    [avatarButton setAction:@selector(buttonClicked:)];
    [avatarButton setRightAction:@selector(buttonRightClicked:)];

    // Check if the account already has an authentication or sync error and
    // initialize the avatar button UI.
    hasError_ = profileObserver_->HasAvatarError();
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
  // Redraw the button if the window has switched between themed and native
  // or if we're in MD design.
  ThemeService* themeService =
      ThemeServiceFactory::GetForProfile(browser_->profile());
  BOOL updatedIsThemedWindow = !themeService->UsingSystemTheme();
  if (isThemedWindow_ != updatedIsThemedWindow ||
      ui::MaterialDesignController::IsModeMaterial()) {
    isThemedWindow_ = updatedIsThemedWindow;
    [[button_ cell] setIsThemedWindow:isThemedWindow_];
    [self updateAvatarButtonAndLayoutParent:YES];
  }
}

- (void)updateAvatarButtonAndLayoutParent:(BOOL)layoutParent {
  // The button text has a black foreground and a white drop shadow for regular
  // windows, and a light text with a dark drop shadow for guest windows
  // which are themed with a dark background. If we're using MD, then there
  // should be no drop shadows.
  BOOL isMaterial = ui::MaterialDesignController::IsModeMaterial();

  NSColor* foregroundColor;
  if (isMaterial) {
    const ui::ThemeProvider* theme =
        &ThemeService::GetThemeProviderForProfile(browser_->profile());
    foregroundColor = theme ? theme->GetNSColor(ThemeProperties::COLOR_TAB_TEXT)
                            : [NSColor blackColor];
  } else {
    foregroundColor = browser_->profile()->IsGuestSession()
                          ? [NSColor colorWithCalibratedWhite:1.0 alpha:0.9]
                          : [NSColor blackColor];
  }

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  // If there is a single local profile, then use the generic avatar button
  // instead of the profile name. Never use the generic button if the active
  // profile is Guest.
  bool useGenericButton =
      !browser_->profile()->IsGuestSession() &&
      storage.GetNumberOfProfiles() == 1 &&
      !storage.GetAllProfilesAttributes().front()->IsAuthenticated();

  NSString* buttonTitle = base::SysUTF16ToNSString(useGenericButton ?
      base::string16() :
      profiles::GetAvatarButtonTextForProfile(browser_->profile()));
  [[button_ cell] setHasError:hasError_ withTitle:buttonTitle];

  AvatarButton* button =
      base::mac::ObjCCastStrict<AvatarButton>(button_);

  if (useGenericButton) {
    if (isMaterial) {
      NSImage* avatarIcon = NSImageFromImageSkia(
          gfx::CreateVectorIcon(gfx::VectorIconId::USER_ACCOUNT_AVATAR, 18,
                                kMaterialAvatarIconColor));
      [button setDefaultImage:avatarIcon];
      [button setHoverImage:nil];
      [button setPressedImage:nil];
    } else {
      [button setDefaultImage:GetImageFromResourceID(
                                  IDR_AVATAR_NATIVE_BUTTON_AVATAR)];
      [button setHoverImage:GetImageFromResourceID(
                                IDR_AVATAR_NATIVE_BUTTON_AVATAR_HOVER)];
      [button setPressedImage:GetImageFromResourceID(
                                  IDR_AVATAR_NATIVE_BUTTON_AVATAR_PRESSED)];
      // This is a workaround for an issue in the HoverImageButton where the
      // button is initially sized incorrectly unless a default image is
      // provided.
      // See crbug.com/298501.
      [button setImage:GetImageFromResourceID(IDR_AVATAR_NATIVE_BUTTON_AVATAR)];
    }
    [button setImagePosition:NSImageOnly];
  } else if (hasError_) {
    NSImage* errorIcon =
        isMaterial
            ? NSImageFromImageSkia(gfx::CreateVectorIcon(
                  gfx::VectorIconId::SYNC_PROBLEM, 16, gfx::kGoogleRed700))
            : GetImageFromResourceID(IDR_ICON_PROFILES_AVATAR_BUTTON_ERROR);
    [button setDefaultImage:errorIcon];
    [button setHoverImage:nil];
    [button setPressedImage:nil];
    [button setImage:errorIcon];
    [button setImagePosition:isMaterial ? NSImageLeft : NSImageRight];
  } else {
    [button setDefaultImage:nil];
    [button setHoverImage:nil];
    [button setPressedImage:nil];
    [button setImagePosition:NSNoImage];
  }

  base::scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setAlignment:NSLeftTextAlignment];

  if (isMaterial) {
    base::scoped_nsobject<NSAttributedString> attributedTitle(
        [[NSAttributedString alloc]
            initWithString:buttonTitle
                attributes:@{
                  NSForegroundColorAttributeName : foregroundColor,
                  NSParagraphStyleAttributeName : paragraphStyle,
                  NSKernAttributeName :
                      [NSNumber numberWithFloat:kMaterialTitleKern]
                }]);
    [button_ setAttributedTitle:attributedTitle];
  } else {
    // Create the white drop shadow.
    base::scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
    [shadow setShadowOffset:NSMakeSize(0, -1)];
    [shadow setShadowBlurRadius:0];
    if (browser_->profile()->IsGuestSession())
      [shadow setShadowColor:[NSColor colorWithCalibratedWhite:0.0 alpha:0.4]];
    else if (!isThemedWindow_)
      [shadow setShadowColor:[NSColor colorWithCalibratedWhite:1.0 alpha:0.7]];
    else
      [shadow setShadowColor:[NSColor colorWithCalibratedWhite:1.0 alpha:0.4]];

    base::scoped_nsobject<NSAttributedString> attributedTitle(
        [[NSAttributedString alloc]
            initWithString:buttonTitle
                attributes:@{
                  NSShadowAttributeName : shadow.get(),
                  NSForegroundColorAttributeName : foregroundColor,
                  NSParagraphStyleAttributeName : paragraphStyle
                }]);
    [button_ setAttributedTitle:attributedTitle];
  }
  [button_ sizeToFit];

  if (layoutParent) {
    // Because the width of the button might have changed, the parent browser
    // frame needs to recalculate the button bounds and redraw it.
    [[BrowserWindowController
        browserWindowControllerForWindow:browser_->window()->GetNativeWindow()]
        layoutSubviews];
  }
}

- (void)setErrorStatus:(BOOL)hasError {
  hasError_ = hasError;
  [self updateAvatarButtonAndLayoutParent:YES];
}

@end
