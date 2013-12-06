// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/avatar_button_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser/avatar_label_button.h"
#import "chrome/browser/ui/cocoa/browser/avatar_menu_bubble_controller.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser/profile_chooser_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

// Space between the avatar icon and the avatar menu bubble.
const CGFloat kMenuYOffsetAdjust = 1.0;

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

@interface AvatarButtonController (Private)
- (void)setButtonEnabled:(BOOL)flag;
- (IBAction)buttonClicked:(id)sender;
- (void)bubbleWillClose:(NSNotification*)notif;
- (NSImage*)compositeImageWithShadow:(NSImage*)image;
- (void)updateAvatar;
- (void)addOrRemoveButtonIfNecessary;
@end

// Declare a 10.7+ private API.
// NSThemeFrame < NSTitledFrame < NSFrameView < NSView.
@interface NSView (NSThemeFrame)
- (void)_tileTitlebarAndRedisplay:(BOOL)redisplay;
@end

namespace AvatarButtonControllerInternal {

class Observer : public content::NotificationObserver {
 public:
  Observer(AvatarButtonController* button) : button_(button) {
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
                   content::NotificationService::AllSources());
  }

  // NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED:
        [button_ updateAvatar];
        [button_ addOrRemoveButtonIfNecessary];
        break;
      default:
        NOTREACHED();
        break;
    }
  }

 private:
  content::NotificationRegistrar registrar_;

  AvatarButtonController* button_;  // Weak; owns this.
};

}  // namespace AvatarButtonControllerInternal

////////////////////////////////////////////////////////////////////////////////

@implementation AvatarButtonController

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super init])) {
    browser_ = browser;

    base::scoped_nsobject<NSView> container(
        [[NSView alloc] initWithFrame:NSMakeRect(
            0, 0, profiles::kAvatarIconWidth, profiles::kAvatarIconHeight)]);
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
      const int icon_id = profile->IsGuestSession() ? IDR_LOGIN_GUEST :
                                                      IDR_OTR_ICON;
      NSImage* icon = ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          icon_id).ToNSImage();
      [self setImage:[self compositeImageWithShadow:icon]];
      [self setButtonEnabled:profile->IsGuestSession()];
   } else {
      [self setButtonEnabled:YES];
      observer_.reset(new AvatarButtonControllerInternal::Observer(self));
      [self updateAvatar];

      // Managed users cannot enter incognito mode, so we only need to check
      // it in this code path.
      if (profile->IsManaged()) {
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

- (void)dealloc {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowWillCloseNotification
              object:[menuController_ window]];
  [super dealloc];
}

- (NSButton*)buttonView {
  return button_.get();
}

- (NSButton*)labelButtonView {
  return labelButton_.get();
}

- (void)setImage:(NSImage*)image {
  [button_ setImage:image];
}

- (void)showAvatarBubble:(NSView*)anchor {
  if (menuController_)
    return;

  DCHECK(chrome::IsCommandEnabled(browser_, IDC_SHOW_AVATAR_MENU));

  NSWindowController* wc =
      [browser_->window()->GetNativeWindow() windowController];
  if ([wc isKindOfClass:[BrowserWindowController class]]) {
    [static_cast<BrowserWindowController*>(wc)
        lockBarVisibilityForOwner:self withAnimation:NO delay:NO];
  }

  NSPoint point = NSMakePoint(NSMidX([anchor bounds]),
                              NSMaxY([anchor bounds]) - kMenuYOffsetAdjust);
  point = [anchor convertPoint:point toView:nil];
  point = [[anchor window] convertBaseToScreen:point];

  // |menuController_| will automatically release itself on close.
  if (profiles::IsNewProfileManagementEnabled()) {
    menuController_ =
      [[ProfileChooserController alloc] initWithBrowser:browser_
                                             anchoredAt:point];
  } else {
    menuController_ =
      [[AvatarMenuBubbleController alloc] initWithBrowser:browser_
                                               anchoredAt:point];
  }
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(bubbleWillClose:)
             name:NSWindowWillCloseNotification
           object:[menuController_ window]];
  [menuController_ showWindow:self];

  ProfileMetrics::LogProfileOpenMethod(ProfileMetrics::ICON_AVATAR_BUBBLE);
}

// Private /////////////////////////////////////////////////////////////////////

- (void)setButtonEnabled:(BOOL)flag {
  [button_ setEnabled:flag];
}

- (IBAction)buttonClicked:(id)sender {
  DCHECK(sender == button_.get() || sender == labelButton_.get());
  [self showAvatarBubble:button_];
}

- (void)bubbleWillClose:(NSNotification*)notif {
  NSWindowController* wc =
      [browser_->window()->GetNativeWindow() windowController];
  if ([wc isKindOfClass:[BrowserWindowController class]]) {
    [static_cast<BrowserWindowController*>(wc)
        releaseBarVisibilityForOwner:self withAnimation:YES delay:NO];
  }
  menuController_ = nil;
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
- (void)updateAvatar {
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
}

// If the second-to-last profile was removed or a second profile was added,
// show or hide the avatar button from the window frame.
- (void)addOrRemoveButtonIfNecessary {
  if (browser_->profile()->IsOffTheRecord())
    return;

  NSWindowController* wc =
      [browser_->window()->GetNativeWindow() windowController];
  if (![wc isKindOfClass:[BrowserWindowController class]])
    return;

  size_t count = g_browser_process->profile_manager()->GetNumberOfProfiles();
  [self.view setHidden:count < 2];

  [static_cast<BrowserWindowController*>(wc) layoutSubviews];

  // If the avatar is being added or removed, then the Lion fullscreen button
  // needs to be adjusted. Since the fullscreen button is positioned by
  // FramedBrowserWindow using private APIs, the easiest way to update the
  // position of the button is through this private API. Resizing the window
  // also works, but invoking |-display| does not.
  NSView* themeFrame = [[[wc window] contentView] superview];
  if ([themeFrame respondsToSelector:@selector(_tileTitlebarAndRedisplay:)])
    [themeFrame _tileTitlebarAndRedisplay:YES];
}

// Testing /////////////////////////////////////////////////////////////////////

- (BaseBubbleController*)menuController {
  return menuController_;
}

@end
