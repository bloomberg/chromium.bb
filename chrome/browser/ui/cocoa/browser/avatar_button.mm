// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/avatar_button.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/image_utils.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#include "chrome/browser/ui/profile_menu_model.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/mac/nsimage_cache.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

@interface AvatarButton (Private)
- (IBAction)buttonClicked:(id)sender;
- (NSImage*)compositeImageWithShadow:(NSImage*)image;
@end

@implementation AvatarButton

- (id)initWithController:(BrowserWindowController*)bwc {
  if ((self = [super init])) {
    // TODO(rsesek): Eventually the PMM will require a Browser so the BWC
    // is plumbed here for that reason. SAIL WILL DO THIS!!1!!1
    controller_ = bwc;
    model_.reset(new ProfileMenuModel());
    menuController_.reset(
        [[MenuController alloc] initWithModel:model_.get()
                       useWithPopUpButtonCell:NO]);

    // This view's single child view is a button with the same size and width as
    // the parent. Set it to automatically resize to the size of this view and
    // to scale the image.
    button_.reset([[NSButton alloc] initWithFrame:[self bounds]]);
    [button_ setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [button_ setButtonType:NSMomentaryLightButton];
    [button_ setImagePosition:NSImageOnly];
    [[button_ cell] setImageScaling:NSImageScaleProportionallyDown];
    [[button_ cell] setImageDimsWhenDisabled:NO];
    [button_ setBordered:NO];
    [button_ setTarget:self];
    [button_ setAction:@selector(buttonClicked:)];
    [self addSubview:button_];
    [self setOpenMenuOnClick:YES];

    if ([controller_ profile]->IsOffTheRecord()) {
      [self setImage:gfx::GetCachedImageWithName(@"otr_icon.pdf")];
    } else if ([controller_ shouldShowAvatar]) {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      [self setImage:rb.GetNativeImageNamed(IDR_PROFILE_AVATAR_1).ToNSImage()];
      [button_ setToolTip:
          base::SysUTF8ToNSString([controller_ profile]->GetProfileName())];
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

- (IBAction)buttonClicked:(id)sender {
  DCHECK_EQ(button_.get(), sender);
  [NSMenu popUpContextMenu:[menuController_ menu]
                 withEvent:[NSApp currentEvent]
                   forView:self];
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

@end
