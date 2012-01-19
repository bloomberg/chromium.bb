// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/previewable_contents_controller.h"

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

@implementation PreviewableContentsController

@synthesize activeContainer = activeContainer_;

- (id)init {
  if ((self = [super initWithNibName:@"PreviewableContents"
                              bundle:base::mac::FrameworkBundle()])) {
  }
  return self;
}

- (void)showPreview:(WebContents*)preview {
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
  // There may be no previewContents_ in the prerender case.
  if (!previewContents_)
    return;

  // Remove the preview contents and reshow the active container.
  [previewContents_->GetNativeView() removeFromSuperview];
  [activeContainer_ setHidden:NO];

  previewContents_ = nil;
}

- (BOOL)isShowingPreview {
  return previewContents_ != nil;
}

@end
