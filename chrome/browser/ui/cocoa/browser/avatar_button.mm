// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/avatar_button.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/browser/avatar_menu_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/image_utils.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#include "chrome/browser/ui/profile_menu_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_service.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/mac/nsimage_cache.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

@interface AvatarButton (Private)
- (IBAction)buttonClicked:(id)sender;
- (NSImage*)compositeImageWithShadow:(NSImage*)image;
- (void)updateAvatar;
@end

namespace AvatarButtonInternal {

class Observer : public NotificationObserver {
 public:
  Observer(AvatarButton* button) : button_(button) {
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
                   NotificationService::AllSources());
  }

  // NotificationObserver:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED:
        [button_ updateAvatar];
        break;
      default:
        NOTREACHED();
        break;
    }
  }

 private:
  NotificationRegistrar registrar_;

  AvatarButton* button_;  // Weak; owns this.
};

}  // namespace AvatarButtonInternal

namespace {

const CGFloat kMenuYOffsetAdjust = 5.0;

}  // namespace

////////////////////////////////////////////////////////////////////////////////

@implementation AvatarButton

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super init])) {
    browser_ = browser;

    // This view's single child view is a button with the same size and width as
    // the parent. Set it to automatically resize to the size of this view and
    // to scale the image.
    button_.reset([[NSButton alloc] initWithFrame:[self bounds]]);
    [button_ setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [button_ setButtonType:NSMomentaryLightButton];
    [button_ setImagePosition:NSImageOnly];
    [[button_ cell] setImageScaling:NSImageScaleProportionallyDown];
    [[button_ cell] setImagePosition:NSImageBelow];
    // AppKit sets a title for some reason when using |-setImagePosition:|.
    [button_ setTitle:nil];
    [[button_ cell] setImageDimsWhenDisabled:NO];
    [[button_ cell] setHighlightsBy:NSContentsCellMask];
    [[button_ cell] setShowsStateBy:NSContentsCellMask];
    [button_ setBordered:NO];
    [button_ setTarget:self];
    [button_ setAction:@selector(buttonClicked:)];
    [self addSubview:button_];
    [self setOpenMenuOnClick:YES];

    if (browser_->profile()->IsOffTheRecord()) {
      [self setImage:gfx::GetCachedImageWithName(@"otr_icon.pdf")];
    } else {
      observer_.reset(new AvatarButtonInternal::Observer(self));
      [self updateAvatar];
    }
  }
  return self;
}

- (void)setOpenMenuOnClick:(BOOL)flag {
  [button_ setEnabled:flag];
}

- (void)setImage:(NSImage*)image {
  [button_ setImage:[self compositeImageWithShadow:image]];
}

// Private /////////////////////////////////////////////////////////////////////

- (IBAction)buttonClicked:(id)sender {
  DCHECK_EQ(button_.get(), sender);

  NSPoint point = NSMakePoint(NSMidX([self bounds]),
                              NSMinY([self bounds]) + kMenuYOffsetAdjust);
  point = [self convertPoint:point toView:nil];
  point = [[self window] convertBaseToScreen:point];

  // |menu| will automatically release itself on close.
  AvatarMenuBubbleController* menu =
      [[AvatarMenuBubbleController alloc] initWithBrowser:browser_
                                               anchoredAt:point];
  [menu showWindow:self];
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
    [self setImage:cache.GetAvatarIconOfProfileAtIndex(index).ToNSImage()];
    const string16& name = cache.GetNameOfProfileAtIndex(index);
    [button_ setToolTip:base::SysUTF16ToNSString(name)];
  }
}

@end
