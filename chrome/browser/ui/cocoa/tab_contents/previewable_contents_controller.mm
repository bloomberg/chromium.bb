// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/previewable_contents_controller.h"

#include "base/mac/bundle_locations.h"
#include "chrome/browser/ui/cocoa/tab_contents/instant_preview_controller_mac.h"
#include "content/public/browser/web_contents.h"

@implementation PreviewableContentsController

@synthesize activeContainer = activeContainer_;

- (id)initWithBrowser:(Browser*)browser
     windowController:(BrowserWindowController*)windowController {
  if ((self = [super initWithNibName:@"PreviewableContents"
                     bundle:base::mac::FrameworkBundle()])) {
    instantPreviewController_.reset(
        new InstantPreviewControllerMac(browser, windowController, self));
  }
  return self;
}

- (void)showPreview:(content::WebContents*)preview {
  DCHECK(preview);

  // Remove any old preview contents before showing the new one.
  if (previewContents_)
    [previewContents_->GetNativeView() removeFromSuperview];

  previewContents_ = preview;
  NSView* previewView = previewContents_->GetNativeView();

  // Set the autoresizing of the the preview correctly.
  [previewView setAutoresizingMask:NSViewHeightSizable|NSViewWidthSizable];
  [previewView setFrame:[[self view] bounds]];

  // Add the preview contents.
  [[self view] addSubview:previewView];
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

@end
