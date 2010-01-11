// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "hover_close_button.h"

#include "app/l10n_util.h"
#include "base/nsimage_cache_mac.h"
#include "grit/generated_resources.h"

namespace  {

const NSString* kNormalImageString = @"close_bar.pdf";
const NSString* kHoverImageString = @"close_bar_h.pdf";
const NSString* kPressedImageString = @"close_bar_p.pdf";

}

@implementation HoverCloseButton

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    [self commonInit];
  }
  return self;
}

- (void)awakeFromNib {
  [self commonInit];
}

- (void)commonInit {
  [self setTrackingEnabled:YES];
  NSImage* alternateImage = nsimage_cache::ImageNamed(kPressedImageString);
  [self setAlternateImage:alternateImage];
  [self updateTrackingAreas];

  // Set accessibility description.
  NSString* description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_CLOSE);
  [[self cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
}

- (void)dealloc {
  [self setTrackingEnabled:NO];
  [super dealloc];
}

- (void)mouseEntered:(NSEvent*)theEvent {
  [self setImage:nsimage_cache::ImageNamed(kHoverImageString)];
}

- (void)mouseExited:(NSEvent*)theEvent {
  [self setImage:nsimage_cache::ImageNamed(kNormalImageString)];
}

- (void)mouseDown:(NSEvent*)theEvent {
  [super mouseDown:theEvent];
  // We need to check the image state after the mouseDown event loop finishes.
  // It's possible that we won't get a mouseExited event if the button was
  // moved under the mouse during tab resize, instead of the mouse moving over
  // the button.
  // http://crbug.com/31279
  [self checkImageState];
}

- (void)setTrackingEnabled:(BOOL)enabled {
  if (enabled) {
    closeTrackingArea_.reset(
        [[NSTrackingArea alloc] initWithRect:[self bounds]
                                     options:NSTrackingMouseEnteredAndExited |
                                             NSTrackingActiveAlways
                                       owner:self
                                    userInfo:nil]);
    [self addTrackingArea:closeTrackingArea_.get()];

    // If you have a separate window that overlaps the close button, and you
    // move the mouse directly over the close button without entering another
    // part of the tab strip, we don't get any mouseEntered event since the
    // tracking area was disabled when we entered.
    [self checkImageState];
  } else {
    if (closeTrackingArea_.get()) {
      [self removeTrackingArea:closeTrackingArea_.get()];
      closeTrackingArea_.reset(nil);
    }
  }
}

- (void)updateTrackingAreas {
  [super updateTrackingAreas];
  [self checkImageState];
}

- (void)checkImageState {
  if (closeTrackingArea_.get()) {
    // Update the close buttons if the tab has moved.
    NSPoint mouseLoc = [[self window] mouseLocationOutsideOfEventStream];
    mouseLoc = [self convertPointFromBase:mouseLoc];
    NSString* name = NSPointInRect(mouseLoc, [self bounds]) ?
        kHoverImageString : kNormalImageString;
    NSImage* newImage = nsimage_cache::ImageNamed(name);
    NSImage* buttonImage = [self image];
    if (![buttonImage isEqual:newImage]) {
      [self setImage:newImage];
    }
  }
}

@end
