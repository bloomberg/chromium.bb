// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/avatar_icon_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_label_button.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

// Space between the avatar label and the left edge of the container containing
// the label and the icon.
const CGFloat kAvatarSpacing = 4;

// Space between the bottom of the avatar icon and the bottom of the avatar
// label.
const CGFloat kAvatarLabelBottomSpacing = 3;

// Space between the right edge of the avatar label and the right edge of the
// avatar icon.
const CGFloat kAvatarLabelRightSpacing = 2;

}  // namespace

@interface AvatarIconController (Private)
- (void)setButtonEnabled:(BOOL)flag;
- (NSImage*)compositeImageWithShadow:(NSImage*)image;
- (void)updateAvatarButtonAndLayoutParent:(BOOL)layoutParent;
- (void)addOrRemoveButtonIfNecessary;
@end

// Declare a 10.7+ private API.
// NSThemeFrame < NSTitledFrame < NSFrameView < NSView.
@interface NSView (NSThemeFrame)
- (void)_tileTitlebarAndRedisplay:(BOOL)redisplay;
@end

@implementation AvatarIconController

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super initWithBrowser:browser])) {
    browser_ = browser;

    base::scoped_nsobject<NSView> container(
        [[NSView alloc] initWithFrame:NSMakeRect(
            0, 0, profiles::kAvatarIconWidth, profiles::kAvatarIconHeight)]);
    [container setWantsLayer:YES];
    [self setView:container];

    button_.reset([[NSButton alloc] initWithFrame:NSMakeRect(
        0, 0, profiles::kAvatarIconWidth, profiles::kAvatarIconHeight)]);
    NSButtonCell* cell = [button_ cell];
    [button_ setButtonType:NSMomentaryLightButton];

    [button_ setImagePosition:NSImageOnly];
    [cell setImageScaling:NSImageScaleProportionallyDown];
    [cell setImagePosition:NSImageBelow];

    // AppKit sets a title for some reason when using |-setImagePosition:|.
    [button_ setTitle:nil];

    [cell setImageDimsWhenDisabled:NO];
    [cell setHighlightsBy:NSContentsCellMask];
    [cell setShowsStateBy:NSContentsCellMask];

    [button_ setBordered:NO];
    [button_ setTarget:self];
    [button_ setAction:@selector(buttonClicked:)];

    [cell accessibilitySetOverrideValue:NSAccessibilityButtonRole
                           forAttribute:NSAccessibilityRoleAttribute];
    [cell accessibilitySetOverrideValue:
        NSAccessibilityRoleDescription(NSAccessibilityButtonRole, nil)
          forAttribute:NSAccessibilityRoleDescriptionAttribute];
    [cell accessibilitySetOverrideValue:
        l10n_util::GetNSString(IDS_PROFILES_BUBBLE_ACCESSIBLE_NAME)
                           forAttribute:NSAccessibilityTitleAttribute];
    [cell accessibilitySetOverrideValue:
        l10n_util::GetNSString(IDS_PROFILES_BUBBLE_ACCESSIBLE_DESCRIPTION)
                           forAttribute:NSAccessibilityHelpAttribute];
    [cell accessibilitySetOverrideValue:
        l10n_util::GetNSString(IDS_PROFILES_BUBBLE_ACCESSIBLE_DESCRIPTION)
                           forAttribute:NSAccessibilityDescriptionAttribute];

    Profile* profile = browser_->profile();

    if (profile->IsOffTheRecord() || profile->IsGuestSession()) {
      const int icon_id = profile->IsGuestSession() ?
          profiles::GetPlaceholderAvatarIconResourceID() : IDR_OTR_ICON;
      NSImage* icon = ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          icon_id).ToNSImage();
      [self setImage:[self compositeImageWithShadow:icon]];
      [self setButtonEnabled:profile->IsGuestSession()];
   } else {
      [self setButtonEnabled:YES];
      [self updateAvatarButtonAndLayoutParent:NO];

      // Supervised users cannot enter incognito mode, so we only need to check
      // it in this code path.
      if (profile->IsSupervised()) {
        // Initialize the avatar label button.
        CGFloat extraWidth =
            profiles::kAvatarIconWidth + kAvatarLabelRightSpacing;
        NSRect frame = NSMakeRect(
            kAvatarSpacing, kAvatarLabelBottomSpacing, extraWidth, 0);
        labelButton_.reset([[AvatarLabelButton alloc] initWithFrame:frame]);
        [labelButton_ setTarget:self];
        [labelButton_ setAction:@selector(buttonClicked:)];
        [[self view] addSubview:labelButton_];

        // Resize the container and reposition the avatar button.
        NSSize textSize = [[labelButton_ cell] labelTextSize];
        [container setFrameSize:
            NSMakeSize([labelButton_ frame].size.width + kAvatarSpacing,
                       profiles::kAvatarIconHeight)];
        [button_
            setFrameOrigin:NSMakePoint(kAvatarSpacing + textSize.width, 0)];
      }
    }
    [[self view] addSubview:button_];
  }
  return self;
}

- (NSButton*)labelButtonView {
  return labelButton_.get();
}

- (void)setImage:(NSImage*)image {
  [button_ setImage:image];
}

- (void)setButtonEnabled:(BOOL)flag {
  [button_ setEnabled:flag];
}

// This will take in an original image and redraw it with a shadow.
- (NSImage*)compositeImageWithShadow:(NSImage*)image {
  gfx::ScopedNSGraphicsContextSaveGState scopedGState;

  base::scoped_nsobject<NSImage> destination(
      [[NSImage alloc] initWithSize:[image size]]);

  NSRect destRect = NSZeroRect;
  destRect.size = [destination size];

  [destination lockFocus];

  base::scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
  [shadow.get() setShadowColor:[NSColor colorWithCalibratedWhite:0.0
                                                           alpha:0.75]];
  [shadow.get() setShadowOffset:NSZeroSize];
  [shadow.get() setShadowBlurRadius:3.0];
  [shadow.get() set];

  [image drawInRect:destRect
           fromRect:NSZeroRect
          operation:NSCompositeSourceOver
           fraction:1.0
     respectFlipped:YES
              hints:nil];

  [destination unlockFocus];

  return destination.autorelease();
}

// Updates the avatar information from the profile cache.
- (void)updateAvatarButtonAndLayoutParent:(BOOL)layoutParent {
  // No updates are needed for an incognito or guest window, as the avatar
  // is always fixed.
  Profile* profile = browser_->profile();
  if (profile->IsOffTheRecord() || profile->IsGuestSession())
    return;

  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t index =
      cache.GetIndexOfProfileWithPath(browser_->profile()->GetPath());
  if (index == std::string::npos)
    return;

  BOOL is_gaia_picture =
      cache.IsUsingGAIAPictureOfProfileAtIndex(index) &&
      cache.GetGAIAPictureOfProfileAtIndex(index);
  gfx::Image icon = profiles::GetAvatarIconForTitleBar(
      cache.GetAvatarIconOfProfileAtIndex(index), is_gaia_picture,
      profiles::kAvatarIconWidth, profiles::kAvatarIconHeight);
  [self setImage:icon.ToNSImage()];

  const base::string16& name = cache.GetNameOfProfileAtIndex(index);
  NSString* nsName = base::SysUTF16ToNSString(name);
  [button_ setToolTip:nsName];
  [[button_ cell]
      accessibilitySetOverrideValue:nsName
                       forAttribute:NSAccessibilityValueAttribute];
  if (layoutParent)
    [self addOrRemoveButtonIfNecessary];
}

// If the second-to-last profile was removed or a second profile was added,
// show or hide the avatar button from the window frame.
- (void)addOrRemoveButtonIfNecessary {
  if (browser_->profile()->IsOffTheRecord())
    return;

  BrowserWindowController* wc = base::mac::ObjCCast<BrowserWindowController>(
      [browser_->window()->GetNativeWindow() windowController]);
  if (!wc)
    return;

  [self.view setHidden:![wc shouldShowAvatar]];

  // If the avatar is being added or removed, then the Lion fullscreen button
  // needs to be adjusted. Since the fullscreen button is positioned by
  // FramedBrowserWindow using private APIs, the easiest way to update the
  // position of the button is through this private API. Resizing the window
  // also works, but invoking |-display| does not.
  NSView* themeFrame = [[[wc window] contentView] superview];
  if ([themeFrame respondsToSelector:@selector(_tileTitlebarAndRedisplay:)])
    [themeFrame _tileTitlebarAndRedisplay:YES];

  // This needs to be called after the fullscreen button is positioned, because
  // TabStripController's rightIndentForControls depends on it.
  [wc layoutSubviews];
}

@end
