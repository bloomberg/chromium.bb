// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/overlayable_contents_controller.h"

#include "base/mac/bundle_locations.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/tab_contents/instant_overlay_controller_mac.h"
#include "chrome/browser/ui/cocoa/tab_contents/overlay_drop_shadow_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

@interface OverlayableContentsController()
- (void)viewDidResize:(NSNotification*)note;
- (void)layoutViews;
- (CGFloat)overlayHeightInPixels;
@end

@implementation OverlayableContentsController

@synthesize drawDropShadow = drawDropShadow_;
@synthesize activeContainerOffset = activeContainerOffset_;

- (id)initWithBrowser:(Browser*)browser
     windowController:(BrowserWindowController*)windowController {
  if ((self = [super init])) {
    windowController_ = windowController;
    scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);
    [view setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];
    [view setAutoresizesSubviews:NO];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(viewDidResize:)
               name:NSViewFrameDidChangeNotification
             object:view];
    [self setView:view];

    activeContainer_.reset([[NSView alloc] initWithFrame:NSZeroRect]);
    [view addSubview:activeContainer_];

    instantOverlayController_.reset(
        new InstantOverlayControllerMac(browser, windowController, self));
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
  // If drawing drop shadow, clip the bottom 1-px-thick separator out of
  // overlay.
  // TODO(sail): remove this when GWS gives chrome the height without the
  // separator.
  if (drawDropShadow && heightUnits != INSTANT_SIZE_PERCENT)
    --height;

  if (overlayContents_ == overlay &&
      overlayHeight_ == height &&
      overlayHeightUnits_ == heightUnits &&
      drawDropShadow_ == drawDropShadow) {
    return;
  }

  // Remove any old overlay contents before showing the new one.
  if (overlayContents_) {
    [overlayContents_->GetView()->GetNativeView() removeFromSuperview];
    overlayContents_->WasHidden();
  }

  overlayContents_ = overlay;
  overlayHeight_ = height;
  overlayHeightUnits_ = heightUnits;
  drawDropShadow_ = drawDropShadow;

  // Add the overlay contents.
  if (overlayContents_) {
    [[[self view] window] disableScreenUpdatesUntilFlush];
    overlayContents_->GetView()->SetAllowOverlappingViews(true);
    [[self view] addSubview:overlayContents_->GetView()->GetNativeView()];
  }

  if (drawDropShadow_) {
    if (!dropShadowView_) {
      dropShadowView_.reset(
          [[OverlayDropShadowView alloc] initWithFrame:NSZeroRect]);
      [[self view] addSubview:dropShadowView_];
    }
  } else {
    [dropShadowView_ removeFromSuperview];
    dropShadowView_.reset();
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

- (BOOL)isShowingOverlay {
  return overlayContents_ != nil;
}

- (InstantOverlayControllerMac*)instantOverlayController {
  return instantOverlayController_.get();
}

- (NSView*)activeContainer {
  return activeContainer_.get();
}

- (NSView*)dropShadowView {
  return dropShadowView_.get();
}

- (void)setActiveContainerOffset:(CGFloat)activeContainerOffset {
  if (activeContainerOffset_ == activeContainerOffset)
    return;

  activeContainerOffset_ = activeContainerOffset;
  [self layoutViews];
}

- (void)viewDidResize:(NSNotification*)note {
  [self layoutViews];
}

- (void)layoutViews {
  NSRect bounds = [[self view] bounds];

  if (overlayContents_) {
    NSRect overlayFrame = bounds;
    overlayFrame.size.height = [self overlayHeightInPixels];
    overlayFrame.origin.y = NSMaxY(bounds) - NSHeight(overlayFrame);
    [overlayContents_->GetView()->GetNativeView() setFrame:overlayFrame];

    if (dropShadowView_) {
      NSRect dropShadowFrame = bounds;
      dropShadowFrame.size.height = [OverlayDropShadowView preferredHeight];
      dropShadowFrame.origin.y =
          NSMinY(overlayFrame) - NSHeight(dropShadowFrame);
      [dropShadowView_ setFrame:dropShadowFrame];
    }
  }

  NSRect activeFrame = bounds;
  activeFrame.size.height -= activeContainerOffset_;
  [activeContainer_ setFrame:activeFrame];
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
