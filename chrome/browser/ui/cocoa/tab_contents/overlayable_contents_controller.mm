// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/overlayable_contents_controller.h"

#include "chrome/browser/ui/cocoa/tab_contents/instant_overlay_controller_mac.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

@interface OverlayableContentsController()
- (void)viewDidResize:(NSNotification*)note;
- (void)layoutViews;
- (CGFloat)overlayHeightInPixels;
@end

@implementation OverlayableContentsController

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super init])) {
    base::scoped_nsobject<NSView> view(
        [[NSView alloc] initWithFrame:NSZeroRect]);
    [view setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(viewDidResize:)
               name:NSViewFrameDidChangeNotification
             object:view];
    [self setView:view];

    activeContainer_.reset([[NSView alloc] initWithFrame:NSZeroRect]);
    [activeContainer_ setAutoresizingMask:NSViewHeightSizable |
                                          NSViewWidthSizable];
    [view addSubview:activeContainer_];

    instantOverlayController_.reset(
        new InstantOverlayControllerMac(browser, self));
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)setOverlay:(content::WebContents*)overlay
            height:(CGFloat)height
       heightUnits:(InstantSizeUnits)heightUnits
    drawDropShadow:(BOOL)drawDropShadow {
  if (overlayContents_ == overlay &&
      overlayHeight_ == height &&
      overlayHeightUnits_ == heightUnits) {
    return;
  }

  // Remove any old overlay contents before showing the new one.
  if (overlayContents_) {
    if (overlayContents_ != overlay)
      overlayContents_->WasHidden();
    [overlayContents_->GetView()->GetNativeView() removeFromSuperview];
  }

  overlayContents_ = overlay;
  overlayHeight_ = height;
  overlayHeightUnits_ = heightUnits;

  // Add the overlay contents.
  if (overlayContents_) {
    [[[self view] window] disableScreenUpdatesUntilFlush];
    [[self view] addSubview:overlayContents_->GetView()->GetNativeView()];
  }

  [self layoutViews];

  if (overlayContents_)
    overlayContents_->WasShown();
}

- (void)onActivateTabWithContents:(content::WebContents*)contents {
  if (overlayContents_ == contents) {
    if (overlayContents_) {
      [overlayContents_->GetView()->GetNativeView() removeFromSuperview];
      overlayContents_ = NULL;
    }
    [self setOverlay:NULL
              height:0
         heightUnits:INSTANT_SIZE_PIXELS
      drawDropShadow:NO];
  }
}

- (InstantOverlayControllerMac*)instantOverlayController {
  return instantOverlayController_.get();
}

- (BOOL)isShowingOverlay {
  return overlayContents_ != nil;
}

- (NSView*)activeContainer {
  return activeContainer_.get();
}

- (void)viewDidResize:(NSNotification*)note {
  [self layoutViews];
}

- (void)layoutViews {
  if (!overlayContents_)
    return;

  // Layout the overlay.
  NSRect bounds = [[self view] bounds];
  NSRect overlayFrame = bounds;
  overlayFrame.size.height = [self overlayHeightInPixels];
  overlayFrame.origin.y = NSMaxY(bounds) - NSHeight(overlayFrame);
  [overlayContents_->GetView()->GetNativeView() setFrame:overlayFrame];
}

- (CGFloat)overlayHeightInPixels {
  CGFloat height = NSHeight([[self view] bounds]);
  switch (overlayHeightUnits_) {
    case INSTANT_SIZE_PERCENT:
      return std::min(height, (height * overlayHeight_) / 100);
    case INSTANT_SIZE_PIXELS:
      return std::min(height, overlayHeight_);
  }
}

@end
