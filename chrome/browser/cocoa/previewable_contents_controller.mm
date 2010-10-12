// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/previewable_contents_controller.h"

#include "base/logging.h"
#include "base/mac_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"

@interface PreviewableContentsController(PrivateMethods)
// Shows or hides the "close preview" button.  Adds the button to the view
// hierarchy, if needed.
- (void)showCloseButton:(BOOL)show;
@end

@implementation PreviewableContentsController

@synthesize activeContainer = activeContainer_;

- (id)init {
  if ((self = [super initWithNibName:@"PreviewableContents"
                              bundle:mac_util::MainAppBundle()])) {
  }
  return self;
}

- (void)showPreview:(TabContents*)preview {
  DCHECK(preview);

  // Remove any old preview contents before showing the new one.
  if (previewContents_)
    [previewContents_->GetNativeView() removeFromSuperview];

  previewContents_ = preview;
  NSView* previewView = previewContents_->GetNativeView();
  [previewView setFrame:[[self view] bounds]];

  // Hide the active container, add the preview contents, and show the tear
  // image.
  [activeContainer_ setHidden:YES];
  [[self view] addSubview:previewView];
  [self showCloseButton:YES];
}

- (void)hidePreview {
  DCHECK(previewContents_);

  // Remove the preview contents, hide the tear image, and reshow the active
  // container.
  [self showCloseButton:NO];
  [previewContents_->GetNativeView() removeFromSuperview];
  [activeContainer_ setHidden:NO];

  previewContents_ = nil;
}

- (IBAction)closePreview:(id)sender {
  // Hiding right now leads to crashes.
  // TODO(rohitrao): Actually hide the preview.
}

- (BOOL)isShowingPreview {
  return previewContents_ != nil;
}

@end

@implementation PreviewableContentsController(PrivateMethods)

- (void)showCloseButton:(BOOL)show {
  if (!show) {
    [closeButton_ removeFromSuperview];
    return;
  }

  if ([closeButton_ superview])
    return;  // Already in the view hierarchy.

  // Add the close button to the upper left corner.
  NSView* view = [self view];
  NSRect frame = [closeButton_ frame];
  frame.origin.x = NSMinX([view bounds]);
  frame.origin.y = NSMaxY([view bounds]) - NSHeight(frame);
  [closeButton_ setFrame:frame];
  [view addSubview:closeButton_];
}

@end
