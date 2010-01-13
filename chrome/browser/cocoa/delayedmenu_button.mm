// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/delayedmenu_button.h"

#include "base/logging.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/clickhold_button_cell.h"

@interface DelayedMenuButton (Private)

- (void)setupCell;
- (void)attachedMenuAction:(id)sender;

@end  // @interface DelayedMenuButton (Private)

@implementation DelayedMenuButton

// Overrides:

+ (Class)cellClass {
  return [ClickHoldButtonCell class];
}

- (id)init {
  if ((self = [super init]))
    [self setupCell];
  return self;
}

- (id)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder]))
    [self setupCell];
  return self;
}

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect]))
    [self setupCell];
  return self;
}

- (void)dealloc {
  [attachedMenu_ release];
  [super dealloc];
}

- (void)awakeFromNib {
  [self setupCell];
}

- (void)setCell:(NSCell*)cell {
  [super setCell:cell];
  [self setupCell];
}

// Accessors and mutators:

@synthesize attachedMenu = attachedMenu_;

// Don't synthesize for attachedMenuEnabled_; its mutator must do other things.
- (void)setAttachedMenuEnabled:(BOOL)enabled {
  attachedMenuEnabled_ = enabled;
  [[self cell] setEnableClickHold:attachedMenuEnabled_];
}

- (BOOL)attachedMenuEnabled {
  return attachedMenuEnabled_;
}

@end  // @implementation DelayedMenuButton

@implementation DelayedMenuButton (Private)

// Set up the button's cell if we've reached a point where it's been set.
- (void)setupCell {
  ClickHoldButtonCell* cell = [self cell];
  if (cell) {
    DCHECK([cell isKindOfClass:[ClickHoldButtonCell class]]);
    [self setEnabled:NO];               // Make the controller put in a menu and
                                        // enable it explicitly. This also takes
                                        // care of |[cell setEnableClickHold:]|.
    [cell setClickHoldAction:@selector(attachedMenuAction:)];
    [cell setClickHoldTarget:self];
  }
}

// Display the menu.
- (void)attachedMenuAction:(id)sender {
  // We shouldn't get here unless the menu is enabled.
  DCHECK(attachedMenuEnabled_);

  // If we don't have a menu (in which case the person using this control is
  // being bad), just wait for a mouse up.
  if (!attachedMenu_) {
    LOG(WARNING) << "No menu available.";
    [NSApp nextEventMatchingMask:NSLeftMouseUpMask
                       untilDate:[NSDate distantFuture]
                          inMode:NSEventTrackingRunLoopMode
                         dequeue:YES];
    return;
  }

  // TODO(viettrungluu): We have some fudge factors below to make things line up
  // (approximately). I wish I knew how to get rid of them. (Note that our view
  // is flipped, and that frame should be in our coordinates.) The y/height is
  // very odd, since it doesn't seem to respond to changes the way that it
  // should. I don't understand it.
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
  // A |scoped_nsobject| local variable cannot be used here because
  // Accessibility on 10.5 grabs the NSPopUpButtonCell without retaining it, and
  // uses it later. (This is fixed in 10.6.)
  if (!popUpCell_.get()) {
    popUpCell_.reset([[NSPopUpButtonCell alloc] initTextCell:@""
                                                   pullsDown:YES]);
  }
  DCHECK(popUpCell_.get());
  [popUpCell_ setMenu:attachedMenu_];
  [popUpCell_ selectItem:nil];
  [popUpCell_ attachPopUpWithFrame:frame
                            inView:self];
  [popUpCell_ performClickWithFrame:frame
                             inView:self];
}

@end  // @implementation DelayedMenuButton (Private)
