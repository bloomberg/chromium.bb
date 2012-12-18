// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/previewable_contents_controller.h"

#include "base/mac/bundle_locations.h"
#include "chrome/browser/ui/cocoa/tab_contents/instant_preview_controller_mac.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

@interface PreviewableContentsController()
- (void)viewDidResize:(NSNotification*)note;
- (void)layoutViews;
- (CGFloat)previewHeightInPixels;
@end

@implementation PreviewableContentsController

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
        heightUnits:(InstantSizeUnits)heightUnits {
  DCHECK(preview);
  if (previewContents_ == preview &&
      previewHeight_ == height &&
      previewHeightUnits_ == heightUnits) {
    return;
  }

  // Remove any old preview contents before showing the new one.
  if (previewContents_)
    [previewContents_->GetNativeView() removeFromSuperview];

  previewContents_ = preview;
  previewHeight_ = height;
  previewHeightUnits_ = heightUnits;

  // Add the preview contents.
  previewContents_->GetView()->SetAllowOverlappingViews(true);
  [[self view] addSubview:previewContents_->GetNativeView()];
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
}

- (void)onActivateTabWithContents:(content::WebContents*)contents {
  if (previewContents_ == contents) {
    [previewContents_->GetNativeView() removeFromSuperview];
    previewContents_ = nil;
  }
}

- (BOOL)isShowingPreview {
  return previewContents_ != nil;
}

- (InstantPreviewControllerMac*)instantPreviewController {
  return instantPreviewController_.get();
}

- (NSView*)activeContainer {
  return activeContainer_.get();
}

- (void)viewDidResize:(NSNotification*)note {
  [self layoutViews];
}

- (void)layoutViews {
  NSRect bounds = [[self view] bounds];

  if (previewContents_) {
    NSRect frame = bounds;
    frame.size.height = [self previewHeightInPixels];
    frame.origin.y = NSMaxY(bounds) - NSHeight(frame);
    [previewContents_->GetNativeView() setFrame:frame];
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
