// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/menu_button.h"

#include "base/logging.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/clickhold_button_cell.h"

@interface MenuButton (Private)

- (void)resetToDefaults;
- (void)showMenu:(BOOL)isDragging;
- (void)clickShowMenu:(id)sender;
- (void)dragShowMenu:(id)sender;

@end  // @interface MenuButton (Private)

@implementation MenuButton

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

// Accessors and mutators:

@synthesize attachedMenu = attachedMenu_;

@end  // @implementation MenuButton

@implementation MenuButton (Private)

// Reset various settings of the button and its associated |ClickHoldButtonCell|
// to the standard state which provides reasonable defaults.
- (void)resetToDefaults {
  ClickHoldButtonCell* cell = [self cell];
  DCHECK([cell isKindOfClass:[ClickHoldButtonCell class]]);
  [cell setEnableClickHold:YES];
  [cell setClickHoldTimeout:0.0];       // Make menu trigger immediately.
  [cell setAction:@selector(clickShowMenu:)];
  [cell setTarget:self];
  [cell setClickHoldAction:@selector(dragShowMenu:)];
  [cell setClickHoldTarget:self];
}

// Actually show the menu (in the correct location). |isDragging| indicates
// whether the mouse button is still down or not.
- (void)showMenu:(BOOL)isDragging {
  if (![self attachedMenu]) {
    LOG(WARNING) << "No menu available.";
    if (isDragging) {
      // If we're dragging, wait for mouse up.
      [NSApp nextEventMatchingMask:NSLeftMouseUpMask
                         untilDate:[NSDate distantFuture]
                            inMode:NSEventTrackingRunLoopMode
                           dequeue:YES];
    }
    return;
  }

  // TODO(viettrungluu): Remove silly fudge factors (same ones as in
  // delayedmenu_button.mm).
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
  [popUpCell_ setMenu:[self attachedMenu]];
  [popUpCell_ selectItem:nil];
  [popUpCell_ attachPopUpWithFrame:frame
                            inView:self];
  [popUpCell_ performClickWithFrame:frame
                             inView:self];
}

// Called when the button is clicked and released. (Shouldn't happen with
// timeout of 0, though there may be some strange pointing devices out there.)
- (void)clickShowMenu:(id)sender {
  [self showMenu:NO];
}

// Called when the button is clicked and dragged/held.
- (void)dragShowMenu:(id)sender {
  [self showMenu:YES];
}

@end  // @implementation MenuButton (Private)
