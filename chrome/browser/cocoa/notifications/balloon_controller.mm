// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/notifications/balloon_controller.h"

#include "app/l10n_util.h"
#import "base/cocoa_protocols_mac.h"
#import "base/scoped_nsobject.h"
#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/cocoa/notifications/balloon_view_host_mac.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

namespace {

// Margin, in pixels, between the notification frame and the contents
// of the notification.
const int kTopMargin = 2;
const int kBottomMargin = 2;
const int kLeftMargin = 2;
const int kRightMargin = 2;

// How many pixels of overlap there is between the shelf top and the
// balloon bottom.
const int kShelfBorderTopOverlap = 6;

// Properties of the dismiss button.
const int kDismissButtonWidth = 52;
const int kDismissButtonHeight = 18;

// Properties of the options menu.
const int kOptionsMenuWidth = 52;
const int kOptionsMenuHeight = 18;

// Properties of the origin label.
const int kLeftLabelMargin = 4;
const int kLabelHeight = 16;

// TODO(johnnyg): http://crbug.com/34826 Add a shadow for the frame.
const int kLeftShadowWidth = 0;
const int kRightShadowWidth = 0;
const int kTopShadowWidth = 0;
const int kBottomShadowWidth = 0;

// The shelf height for the system default font size.  It is scaled
// with changes in the default font size.
const int kDefaultShelfHeight = 22;

}  // namespace

@interface BalloonController (InternalLayout)
// The following are all internal methods to calculate the dimensions of
// subviews.
- (NSPoint)contentsOffset;
- (int)shelfHeight;
- (int)balloonFrameHeight;
- (NSRect)contentsRectangle;
- (NSRect)closeButtonBounds;
- (NSRect)optionsMenuBounds;
- (NSRect)labelBounds;
@end

@implementation BalloonController

- (id)initWithBalloon:(Balloon*)balloon {
  NSString* sourceLabelText = l10n_util::GetNSStringF(
      IDS_NOTIFICATION_BALLOON_SOURCE_LABEL,
      WideToUTF16(balloon->notification().display_source()));
  NSString* optionsText =
      l10n_util::GetNSString(IDS_NOTIFICATION_OPTIONS_MENU_LABEL);
  NSString* dismissText =
      l10n_util::GetNSString(IDS_NOTIFICATION_BALLOON_DISMISS_LABEL);
  NSString* revokeText =
      l10n_util::GetNSStringF(IDS_NOTIFICATION_BALLOON_REVOKE_MESSAGE,
          WideToUTF16(balloon->notification().display_source()));

  balloon_ = balloon;

  frameContainer_.reset([[NSWindow alloc]
      initWithContentRect:NSZeroRect
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:YES]);
  [frameContainer_ setAlphaValue:1.0];
  [frameContainer_ setOpaque:NO];
  [frameContainer_ setLevel:NSFloatingWindowLevel];

  if ((self = [self initWithWindow:frameContainer_.get()])) {
    // Position of the balloon.
    int x = balloon_->position().x();
    int y = balloon_->position().y();
    int w = [self desiredTotalWidth];
    int h = [self desiredTotalHeight];

    NSView* contentView = [frameContainer_ contentView];

    frameView_.reset([[BalloonViewCocoa alloc]
        initWithFrame:NSMakeRect(0,
                                 [self shelfHeight],
                                 w,
                                 [self balloonFrameHeight])]);
    [contentView addSubview:frameView_.get()];

    htmlContainer_.reset(
        [[NSView alloc] initWithFrame:[self contentsRectangle]]);
    [htmlContainer_ setNeedsDisplay:YES];
    [htmlContainer_ setFrame:[self contentsRectangle]];
    [frameView_ addSubview:htmlContainer_.get()];

    htmlContents_.reset(new BalloonViewHost(balloon));
    [self initializeHost];

    scoped_nsobject<NSView> shelfView([[BalloonShelfViewCocoa alloc]
        initWithFrame:NSMakeRect(0,
                                 0,
                                 w,
                                 [self shelfHeight]+kShelfBorderTopOverlap)]);
    [contentView addSubview:shelfView.get()
                 positioned:NSWindowBelow
                 relativeTo:htmlContainer_.get()];

    optionsMenu_.reset([[NSMenu alloc] init]);
    [optionsMenu_ addItemWithTitle:revokeText
                            action:@selector(permissionRevoked:)
                     keyEquivalent:@""];


    // Creating UI elements by hand for easier parity with other platforms.
    // TODO(johnnyg): http://crbug.com/34627
    // Investigate converting this to a nib.
    scoped_nsobject<BalloonButtonCell> closeButtonCell(
        [[BalloonButtonCell alloc] initTextCell:dismissText]);
    scoped_nsobject<NSButton> closeButton(
        [[NSButton alloc] initWithFrame:[self closeButtonBounds]]);
    [shelfView addSubview:closeButton.get()];
    [closeButton setCell:closeButtonCell.get()];
    [closeButton setTarget:self];
    [closeButton setAction:@selector(closeButtonPressed:)];
    [closeButtonCell setTextColor:[NSColor whiteColor]];

    scoped_nsobject<BalloonButtonCell> optionsButtonCell(
        [[BalloonButtonCell alloc] initTextCell:optionsText]);
    scoped_nsobject<NSButton> optionsButton(
        [[NSButton alloc] initWithFrame:[self optionsMenuBounds]]);
    [shelfView addSubview:optionsButton.get()];
    [optionsButton setCell:optionsButtonCell.get()];
    [optionsButton setTarget:self];
    [optionsButton setAction:@selector(optionsButtonPressed:)];
    [optionsButtonCell setTextColor:[NSColor whiteColor]];

    scoped_nsobject<NSTextField> originLabel([[NSTextField alloc] init]);
    [shelfView addSubview:originLabel.get()];
    [originLabel setEditable:NO];
    [originLabel setBezeled:NO];
    [originLabel setSelectable:NO];
    [originLabel setDrawsBackground:NO];
    [[originLabel cell] setLineBreakMode:NSLineBreakByTruncatingTail];
    [[originLabel cell] setTextColor:[NSColor whiteColor]];
    [originLabel setStringValue:sourceLabelText];
    [originLabel setFrame:[self labelBounds]];
    [originLabel setFont:
        [NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];

    [frameContainer_ setFrame:NSMakeRect(x, y, w, h) display:YES];
    [frameContainer_ setDelegate:self];
  }
  return self;
}

- (IBAction)closeButtonPressed:(id)sender {
  [self closeBalloon:YES];
}

- (IBAction)optionsButtonPressed:(id)sender {
  [NSMenu popUpContextMenu:optionsMenu_
                 withEvent:[NSApp currentEvent]
                   forView:frameView_];
}

- (IBAction)permissionRevoked:(id)sender {
  DesktopNotificationService* service =
      balloon_->profile()->GetDesktopNotificationService();
  service->DenyPermission(balloon_->notification().origin_url());
}

- (void)closeBalloon:(bool)byUser {
  DCHECK(balloon_);
  [self close];
  htmlContents_->Shutdown();
  balloon_->OnClose(byUser);
  balloon_ = NULL;
}

- (void)repositionToBalloon {
  DCHECK(balloon_);
  int x = balloon_->position().x();
  int y = balloon_->position().y();
  int w = [self desiredTotalWidth];
  int h = [self desiredTotalHeight];

  NSRect frame = NSMakeRect(x, y, w, h);

  htmlContents_->UpdateActualSize(balloon_->content_size());
  [htmlContainer_ setFrame:[self contentsRectangle]];
  [frameView_ setFrame:NSMakeRect(0,
                                  [self shelfHeight],
                                  w,
                                  [self balloonFrameHeight])];

  [animation_ stopAnimation];

  NSDictionary* dict =
      [NSDictionary dictionaryWithObjectsAndKeys:
           frameContainer_.get(), NSViewAnimationTargetKey,
           [NSValue valueWithRect:frame], NSViewAnimationEndFrameKey, nil];

  animation_.reset([[NSViewAnimation alloc]
      initWithViewAnimations:[NSArray arrayWithObjects:dict, nil]]);
  [animation_ startAnimation];
}

// Returns the total width the view should be to accommodate the balloon.
- (int)desiredTotalWidth {
  return (balloon_ ? balloon_->content_size().width() : 0) +
      kLeftMargin + kRightMargin + kLeftShadowWidth + kRightShadowWidth;
}

// Returns the total height the view should be to accommodate the balloon.
- (int)desiredTotalHeight {
  return (balloon_ ? balloon_->content_size().height() : 0) +
      kTopMargin + kBottomMargin + kTopShadowWidth + kBottomShadowWidth +
      [self shelfHeight];
}

// Returns the BalloonHost {
- (BalloonViewHost*) getHost {
  return htmlContents_.get();
}

// Relative to the lower left of the frame, above the shelf.
- (NSPoint)contentsOffset {
  return NSMakePoint(kLeftShadowWidth + kLeftMargin,
                     kBottomShadowWidth + kBottomMargin);
}

// Returns the height of the shelf in pixels.
- (int)shelfHeight {
  // TODO(johnnyg): add scaling here.
  return kDefaultShelfHeight;
}

// Returns the height of the balloon contents frame in pixels.
- (int)balloonFrameHeight {
  return [self desiredTotalHeight] - [self shelfHeight];
}

// Relative to the lower-left of the frame.
- (NSRect)contentsRectangle {
  gfx::Size contentSize = balloon_->content_size();
  NSPoint offset = [self contentsOffset];
  return NSMakeRect(offset.x, offset.y,
                    contentSize.width(), contentSize.height());
}

// Returns the bounds of the close button.
- (NSRect)closeButtonBounds {
  return NSMakeRect(
      [self desiredTotalWidth] - kDismissButtonWidth - kRightMargin,
      kBottomMargin,
      kDismissButtonWidth,
      kDismissButtonHeight);
}

// Returns the bounds of the button which opens the options menu.
- (NSRect)optionsMenuBounds {
  return NSMakeRect(
      [self desiredTotalWidth] -
          kDismissButtonWidth - kOptionsMenuWidth - kRightMargin,
      kBottomMargin,
      kOptionsMenuWidth,
      kOptionsMenuHeight);
}

// Returns the bounds of the label showing the origin of the notification.
- (NSRect)labelBounds {
  return NSMakeRect(
      kLeftLabelMargin,
      kBottomMargin,
      [self desiredTotalWidth] -
          kDismissButtonWidth - kOptionsMenuWidth - kRightMargin,
      kLabelHeight);
}

// Initializes the renderer host showing the HTML contents.
- (void)initializeHost {
  htmlContents_->Init();
  gfx::NativeView contents = htmlContents_->native_view();
  [htmlContainer_ addSubview:contents];
}

// NSWindowDelegate notification.
- (void)windowWillClose:(NSNotification*)notif {
  [self autorelease];
}

@end
