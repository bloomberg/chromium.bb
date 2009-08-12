// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/delayedmenu_button.h"

#include "base/logging.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/clickhold_button_cell.h"

@interface DelayedMenuButton (Private)

- (void)resetToDefaults;
- (void)menuAction:(id)sender;

@end  // @interface DelayedMenuButton (Private)

@implementation DelayedMenuButton

// Overrides:

+ (Class)cellClass {
  return [ClickHoldButtonCell class];
}

- (id)init {
  if ((self = [super init]))
    [self resetToDefaults];
  return self;
}

- (id)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder]))
    [self resetToDefaults];
  return self;
}

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect]))
    [self resetToDefaults];
  return self;
}

- (void)dealloc {
  [menu_ release];
  [super dealloc];
}

- (void)awakeFromNib {
  [self resetToDefaults];
}

// Accessors and mutators:

@synthesize menu = menu_;

// Don't synthesize for menuEnabled_; its mutator must do other things.
- (void)setMenuEnabled:(BOOL)enabled {
  menuEnabled_ = enabled;
  [[self cell] setEnableClickHold:menuEnabled_];
}

- (BOOL)menuEnabled {
  return menuEnabled_;
}

@end  // @implementation DelayedMenuButton

@implementation DelayedMenuButton (Private)

- (void)resetToDefaults {
  id cell = [self cell];
  DCHECK([cell isKindOfClass:[ClickHoldButtonCell class]]);
  [self setEnabled:NO];                 // Make the controller put in a menu and
                                        // enable it explicitly. This also takes
                                        // care of |[cell setEnableClickHold:]|.
  [cell setClickHoldTimeout:0.25];      // Random guess at Cocoa-ish value.
  [cell setTrackOnlyInRect:NO];
  [cell setActivateOnDrag:YES];
  [cell setClickHoldAction:@selector(menuAction:)];
  [cell setClickHoldTarget:self];
}

- (void)menuAction:(id)sender {
  // We shouldn't get here unless the menu is enabled.
  DCHECK(menuEnabled_);

  // If we don't have a menu (in which case the person using this control is
  // being bad), just wait for a mouse up.
  if (!menu_) {
    LOG(WARNING) << "No menu available.";
    [NSApp nextEventMatchingMask:NSLeftMouseUpMask
                       untilDate:[NSDate distantFuture]
                          inMode:NSEventTrackingRunLoopMode
                         dequeue:YES];
    return;
  }

  // FIXME(viettrungluu@gmail.com): We have some fudge factors below to make
  // things line up (approximately). I wish I knew how to get rid of them. (Note
  // that our view is flipped, and that frame should be in our coordinates.)
  // The y/height is very odd, since it doesn't seem to respond to changes the
  // way that it should. I don't understand it.
  NSRect frame = [self convertRect:[self frame]
                          fromView:[self superview]];
  frame.origin.x -= 2.0;
  frame.size.height += 10.0;

  // Make our pop-up button cell and set things up. This is, as of 10.5, the
  // official Apple-recommended hack. Later, perhaps |-[NSMenu
  // popUpMenuPositioningItem:atLocation:inView:]| may be a better option.
  // However, using a pulldown has the benefit that Cocoa automatically places
  // the menu correctly even when we're at the edge of the screen (including
  // "dragging upwards" when the button is close to the bottom of the screen).
  scoped_nsobject<NSPopUpButtonCell> popUpCell(
      [[NSPopUpButtonCell alloc] initTextCell:@""
                                    pullsDown:YES]);
  DCHECK(popUpCell.get());
  [popUpCell setMenu:menu_];
  [popUpCell selectItem:nil];
  [popUpCell attachPopUpWithFrame:frame
                           inView:self];
  [popUpCell performClickWithFrame:frame
                            inView:self];
}

@end  // @implementation DelayedMenuButton (Private)
