// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_controller.h"

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac2.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"

@interface ConstrainedWindowController ()
- (void)onEmbeddedViewFrameDidChange:(NSNotification*)note;
@end

@implementation ConstrainedWindowController

- (id)initWithParentWebContents:(content::WebContents*)parentWebContents
                   embeddedView:(NSView*)embeddedView {
  scoped_nsobject<NSWindow> window([[ConstrainedWindowCustomWindow alloc]
      initWithContentRect:[embeddedView bounds]]);
  if ((self = [super initWithWindow:window])) {
    embeddedView_.reset([embeddedView retain]);
    [[window contentView] addSubview:embeddedView];

    [embeddedView setPostsFrameChangedNotifications:YES];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onEmbeddedViewFrameDidChange:)
               name:NSViewFrameDidChangeNotification
             object:embeddedView];

    constrainedWindow_ = new ConstrainedWindowMac2(
        TabContents::FromWebContents(parentWebContents), window);
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)close {
  if (constrainedWindow_) {
    constrainedWindow_->CloseConstrainedWindow();
    constrainedWindow_ = NULL;
  }
}

- (void)onEmbeddedViewFrameDidChange:(NSNotification*)note {
  NSSize newSize = [embeddedView_ frame].size;
  [[ConstrainedWindowSheetController controllerForSheet:[self window]]
        setSheet:[self window]
      windowSize:newSize];
}

@end
