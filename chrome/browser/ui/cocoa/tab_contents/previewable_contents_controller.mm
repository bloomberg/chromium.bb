// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/previewable_contents_controller.h"

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "content/browser/tab_contents/tab_contents.h"

@implementation PreviewableContentsController

@synthesize activeContainer = activeContainer_;

- (id)init {
  if ((self = [super initWithNibName:@"PreviewableContents"
                              bundle:base::mac::MainAppBundle()])) {
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

  // Hide the active container and add the preview contents.
  [activeContainer_ setHidden:YES];
  [[self view] addSubview:previewView];
}

- (void)hidePreview {
  DCHECK(previewContents_);

  // Remove the preview contents and reshow the active container.
  [previewContents_->GetNativeView() removeFromSuperview];
  [activeContainer_ setHidden:NO];

  previewContents_ = nil;
}

- (BOOL)isShowingPreview {
  return previewContents_ != nil;
}

@end
