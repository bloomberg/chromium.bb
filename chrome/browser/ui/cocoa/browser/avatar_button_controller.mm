// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/avatar_button_controller.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser/avatar_menu_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/image_utils.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/mac/nsimage_cache.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

@interface AvatarButtonController (Private)
- (void)setOpenMenuOnClick:(BOOL)flag;
- (IBAction)buttonClicked:(id)sender;
- (void)bubbleWillClose:(NSNotification*)notif;
- (NSImage*)compositeImageWithShadow:(NSImage*)image;
- (void)updateAvatar;
- (void)addOrRemoveButtonIfNecessary;
@end

namespace AvatarButtonControllerInternal {

class Observer : public content::NotificationObserver {
 public:
  Observer(AvatarButtonController* button) : button_(button) {
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
                   content::NotificationService::AllSources());
  }

  // NotificationObserver:
  void Observe(int type,
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

namespace {

const CGFloat kMenuYOffsetAdjust = 1.0;

}  // namespace

////////////////////////////////////////////////////////////////////////////////

@implementation AvatarButtonController

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super init])) {
    browser_ = browser;

    // This view's single child view is a button with the same size and width as
    // the parent. Set it to automatically resize to the size of this view and
    // to scale the image.
    scoped_nsobject<NSButton> button(
        [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 20, 20)]);
    [button setButtonType:NSMomentaryLightButton];

    [button setImagePosition:NSImageOnly];
    [[button cell] setImageScaling:NSImageScaleProportionallyDown];
    [[button cell] setImagePosition:NSImageBelow];

    // AppKit sets a title for some reason when using |-setImagePosition:|.
    [button setTitle:nil];

    [[button cell] setImageDimsWhenDisabled:NO];
    [[button cell] setHighlightsBy:NSContentsCellMask];
    [[button cell] setShowsStateBy:NSContentsCellMask];

    [button setBordered:NO];
    [button setTarget:self];
    [button setAction:@selector(buttonClicked:)];

    NSButtonCell* cell = [button cell];
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

    [self setView:button];

    if (browser_->profile()->IsOffTheRecord()) {
      [self setImage:[self compositeImageWithShadow:
          gfx::GetCachedImageWithName(@"otr_icon.pdf")]];
      [self setOpenMenuOnClick:NO];
    } else {
      observer_.reset(new AvatarButtonControllerInternal::Observer(self));
      [self setOpenMenuOnClick:YES];
      [self updateAvatar];
    }
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
  return static_cast<NSButton*>(self.view);
}

- (void)setImage:(NSImage*)image {
  [self.buttonView setImage:image];
}

- (void)showAvatarBubble {
  if (menuController_)
    return;

  NSWindowController* wc =
      [browser_->window()->GetNativeHandle() windowController];
  if ([wc isKindOfClass:[BrowserWindowController class]]) {
    [static_cast<BrowserWindowController*>(wc)
        lockBarVisibilityForOwner:self withAnimation:NO delay:NO];
  }

  NSView* view = self.view;
  NSPoint point = NSMakePoint(NSMidX([view bounds]),
                              NSMaxY([view bounds]) - kMenuYOffsetAdjust);
  point = [view convertPoint:point toView:nil];
  point = [[view window] convertBaseToScreen:point];

  // |menu| will automatically release itself on close.
  menuController_ = [[AvatarMenuBubbleController alloc] initWithBrowser:browser_
                                                             anchoredAt:point];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(bubbleWillClose:)
             name:NSWindowWillCloseNotification
           object:[menuController_ window]];
  [menuController_ showWindow:self];

  ProfileMetrics::LogProfileOpenMethod(ProfileMetrics::ICON_AVATAR_BUBBLE);
}

// Private /////////////////////////////////////////////////////////////////////

- (void)setOpenMenuOnClick:(BOOL)flag {
  [self.buttonView setEnabled:flag];
}

- (IBAction)buttonClicked:(id)sender {
  DCHECK_EQ(self.buttonView, sender);
  [self showAvatarBubble];
}

- (void)bubbleWillClose:(NSNotification*)notif {
  NSWindowController* wc =
      [browser_->window()->GetNativeHandle() windowController];
  if ([wc isKindOfClass:[BrowserWindowController class]]) {
    [static_cast<BrowserWindowController*>(wc)
        releaseBarVisibilityForOwner:self withAnimation:YES delay:NO];
  }
  menuController_ = nil;
}

// This will take in an original image and redraw it with a shadow.
- (NSImage*)compositeImageWithShadow:(NSImage*)image {
  gfx::ScopedNSGraphicsContextSaveGState scopedGState;

  scoped_nsobject<NSImage> destination(
      [[NSImage alloc] initWithSize:[image size]]);

  NSRect destRect = NSZeroRect;
  destRect.size = [destination size];

  [destination lockFocus];

  scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
  [shadow.get() setShadowColor:[NSColor colorWithCalibratedWhite:0.0
                                                           alpha:0.75]];
  [shadow.get() setShadowOffset:NSMakeSize(0, 0)];
  [shadow.get() setShadowBlurRadius:3.0];
  [shadow.get() set];

  [image drawInRect:destRect
           fromRect:NSZeroRect
          operation:NSCompositeSourceOver
           fraction:1.0
       neverFlipped:YES];

  [destination unlockFocus];

  return [destination.release() autorelease];
}

// Updates the avatar information from the profile cache.
- (void)updateAvatar {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t index =
      cache.GetIndexOfProfileWithPath(browser_->profile()->GetPath());
  if (index != std::string::npos) {
    BOOL is_gaia_picture =
        cache.IsUsingGAIAPictureOfProfileAtIndex(index) &&
        cache.GetGAIAPictureOfProfileAtIndex(index);
    gfx::Image icon = profiles::GetAvatarIconForTitleBar(
        cache.GetAvatarIconOfProfileAtIndex(index), is_gaia_picture,
        profiles::kAvatarIconWidth, profiles::kAvatarIconHeight);
    [self setImage:icon.ToNSImage()];

    const string16& name = cache.GetNameOfProfileAtIndex(index);
    NSString* nsName = base::SysUTF16ToNSString(name);
    [self.view setToolTip:nsName];
    [[self.buttonView cell]
        accessibilitySetOverrideValue:nsName
                         forAttribute:NSAccessibilityValueAttribute];
  }
}

// If the second-to-last profile was removed or a second profile was added,
// show or hide the avatar button from the window frame.
- (void)addOrRemoveButtonIfNecessary {
  if (browser_->profile()->IsOffTheRecord())
    return;

  NSWindowController* wc =
      [browser_->window()->GetNativeHandle() windowController];
  if (![wc isKindOfClass:[BrowserWindowController class]])
    return;

  size_t count = g_browser_process->profile_manager()->GetNumberOfProfiles();
  [self.view setHidden:count < 2];

  [static_cast<BrowserWindowController*>(wc) layoutSubviews];
}

// Testing /////////////////////////////////////////////////////////////////////

- (AvatarMenuBubbleController*)menuController {
  return menuController_;
}

@end
