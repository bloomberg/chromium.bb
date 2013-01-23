// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/previewable_contents_controller.h"

#include "base/mac/bundle_locations.h"
#include "chrome/browser/ui/cocoa/tab_contents/instant_preview_controller_mac.h"
#include "chrome/browser/ui/cocoa/tab_contents/preview_drop_shadow_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

@interface PreviewableContentsController()
- (void)viewDidResize:(NSNotification*)note;
- (void)layoutViews;
- (CGFloat)previewHeightInPixels;
@end

@implementation PreviewableContentsController

@synthesize drawDropShadow = drawDropShadow_;

- (id)initWithBrowser:(Browser*)browser
     windowController:(BrowserWindowController*)windowController {
  if ((self = [super init])) {
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

    instantPreviewController_.reset(
        new InstantPreviewControllerMac(browser, windowController, self));
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)showPreview:(content::WebContents*)preview
             height:(CGFloat)height
        heightUnits:(InstantSizeUnits)heightUnits
     drawDropShadow:(BOOL)drawDropShadow {
  DCHECK(preview);

  // If drawing drop shadow, clip the bottom 1-px-thick separator out of
  // preview.
  // TODO(sail): remove this when GWS gives chrome the height without the
  // separator.
  if (drawDropShadow && heightUnits != INSTANT_SIZE_PERCENT)
    --height;

  if (previewContents_ == preview &&
      previewHeight_ == height &&
      previewHeightUnits_ == heightUnits &&
      drawDropShadow_ == drawDropShadow) {
    return;
  }

  // Remove any old preview contents before showing the new one.
  if (previewContents_)
    [previewContents_->GetNativeView() removeFromSuperview];

  previewContents_ = preview;
  previewHeight_ = height;
  previewHeightUnits_ = heightUnits;
  drawDropShadow_ = drawDropShadow;

  // Add the preview contents.
  [[[self view] window] disableScreenUpdatesUntilFlush];
  previewContents_->GetView()->SetAllowOverlappingViews(true);
  [[self view] addSubview:previewContents_->GetNativeView()];

  if (drawDropShadow_) {
    if (!dropShadowView_) {
      dropShadowView_.reset(
          [[PreviewDropShadowView alloc] initWithFrame:NSZeroRect]);
      [[self view] addSubview:dropShadowView_];
    }
  } else {
    [dropShadowView_ removeFromSuperview];
    dropShadowView_.reset();
  }

  [self layoutViews];

  previewContents_->WasShown();
}

- (void)hidePreview {
  // There may be no previewContents_ in the prerender case.
  if (!previewContents_)
    return;

  // Remove the preview contents.
  [previewContents_->GetNativeView() removeFromSuperview];
  previewContents_->WasHidden();
  previewContents_ = nil;

  drawDropShadow_ = false;
  [dropShadowView_ removeFromSuperview];
  dropShadowView_.reset();
}

- (void)onActivateTabWithContents:(content::WebContents*)contents {
  if (previewContents_ == contents) {
    [previewContents_->GetNativeView() removeFromSuperview];
    previewContents_ = nil;
  }
}

- (InstantPreviewControllerMac*)instantPreviewController {
  return instantPreviewController_.get();
}

- (NSView*)activeContainer {
  return activeContainer_.get();
}

- (NSView*)dropShadowView {
  return dropShadowView_.get();
}

- (void)viewDidResize:(NSNotification*)note {
  [self layoutViews];
}

- (void)layoutViews {
  NSRect bounds = [[self view] bounds];

  if (previewContents_) {
    NSRect previewFrame = bounds;
    previewFrame.size.height = [self previewHeightInPixels];
    previewFrame.origin.y = NSMaxY(bounds) - NSHeight(previewFrame);
    [previewContents_->GetNativeView() setFrame:previewFrame];

    if (dropShadowView_) {
      NSRect dropShadowFrame = bounds;
      dropShadowFrame.size.height = [PreviewDropShadowView preferredHeight];
      dropShadowFrame.origin.y =
          NSMinY(previewFrame) - NSHeight(dropShadowFrame);
      [dropShadowView_ setFrame:dropShadowFrame];
    }
  }

  [activeContainer_ setFrame:bounds];
}

- (CGFloat)previewHeightInPixels {
  CGFloat height = NSHeight([[self view] bounds]);
  switch (previewHeightUnits_) {
    case INSTANT_SIZE_PERCENT:
      return std::min(height, (height * previewHeight_) / 100);
    case INSTANT_SIZE_PIXELS:
      return std::min(height, previewHeight_);
  }
}

@end
