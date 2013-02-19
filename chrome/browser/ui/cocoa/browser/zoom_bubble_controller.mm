// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/browser/zoom_bubble_controller.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chrome_page_zoom.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "content/public/common/page_zoom.h"
#include "grit/generated_resources.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"

@interface ZoomBubbleController (Private)
- (void)performLayout;
- (void)autoCloseBubble;
@end

namespace {

// The amount of time to wait before the bubble automatically closes.
// Should keep in sync with kBubbleCloseDelay in
// src/chrome/browser/ui/views/location_bar/zoom_bubble_view.cc.
const NSTimeInterval kAutoCloseDelay = 1.5;

// The amount of padding between the window frame and controls.
const CGFloat kPadding = 10.0;

}  // namespace

@implementation ZoomBubbleController

- (id)initWithParentWindow:(NSWindow*)parentWindow
             closeObserver:(void(^)(ZoomBubbleController*))closeObserver {
  scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 200, 100)
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  if ((self = [super initWithWindow:window
                       parentWindow:parentWindow
                         anchoredAt:NSZeroPoint])) {
    closeObserver_.reset(Block_copy(closeObserver));
    [self performLayout];
  }
  return self;
}

- (void)showForWebContents:(content::WebContents*)contents
                anchoredAt:(NSPoint)anchorPoint
                 autoClose:(BOOL)autoClose {
  contents_ = contents;
  [self onZoomChanged];

  self.anchorPoint = anchorPoint;
  [self showWindow:nil];

  autoClose_ = autoClose;
  if (autoClose_) {
    [self performSelector:@selector(autoCloseBubble)
               withObject:nil
               afterDelay:kAutoCloseDelay];
  }
}

- (void)onZoomChanged {
  if (!contents_)
    return;  // NULL in tests.

  ZoomController* zoomController = ZoomController::FromWebContents(contents_);
  int percent = zoomController->zoom_percent();
  [zoomPercent_ setStringValue:
      l10n_util::GetNSStringF(IDS_TOOLTIP_ZOOM, base::IntToString16(percent))];
}

- (void)resetToDefault:(id)sender {
  [self close];
  chrome_page_zoom::Zoom(contents_, content::PAGE_ZOOM_RESET);
}

- (void)windowWillClose:(NSNotification*)notification {
  contents_ = NULL;
  closeObserver_.get()(self);
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(autoCloseBubble)
                                             object:nil];
  [super windowWillClose:notification];
}

// Private /////////////////////////////////////////////////////////////////////

- (void)performLayout {
  NSView* parent = [[self window] contentView];

  // Create the reset zoom button.
  scoped_nsobject<NSButton> resetButton(
      [[NSButton alloc] initWithFrame:NSMakeRect(0, kPadding, 0, 0)]);
  [resetButton setTitle:l10n_util::GetNSStringWithFixup(IDS_ZOOM_SET_DEFAULT)];
  [resetButton setButtonType:NSMomentaryPushInButton];
  [[resetButton cell] setControlSize:NSSmallControlSize];
  [resetButton setBezelStyle:NSRoundedBezelStyle];
  [resetButton setTarget:self];
  [resetButton setAction:@selector(resetToDefault:)];
  [resetButton sizeToFit];

  // Center it within the window.
  NSRect buttonFrame = [resetButton frame];
  buttonFrame.origin.x = NSMidX([parent frame]) - NSMidX(buttonFrame);
  [resetButton setFrame:buttonFrame];
  [parent addSubview:resetButton];

  NSRect windowFrame = [[self window] frame];

  // Create the label to display the current zoom amount.
  zoomPercent_.reset([[NSTextField alloc] initWithFrame:
      NSMakeRect(kPadding, NSMaxY(buttonFrame),
                 NSWidth(windowFrame) - 2 * kPadding, 22)]);
  [zoomPercent_ setEditable:NO];
  [zoomPercent_ setBezeled:NO];
  [zoomPercent_ setAlignment:NSCenterTextAlignment];
  [parent addSubview:zoomPercent_];

  // Adjust the height of the window to fit the content.
  windowFrame.size.height = NSMaxY([zoomPercent_ frame]) + kPadding +
      info_bubble::kBubbleArrowHeight;
  [[self window] setFrame:windowFrame display:YES];
}

- (void)autoCloseBubble {
  if (!autoClose_)
    return;
  [self close];
}

@end
