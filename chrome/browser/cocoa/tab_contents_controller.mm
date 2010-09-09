// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_contents_controller.h"

#include "base/mac_util.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"


@implementation TabContentsController
@synthesize tabContents = contents_;

- (id)initWithNibName:(NSString*)name
             contents:(TabContents*)contents {
  if ((self = [super initWithNibName:name
                              bundle:mac_util::MainAppBundle()])) {
    contents_ = contents;
  }
  return self;
}

- (void)dealloc {
  // make sure our contents have been removed from the window
  [[self view] removeFromSuperview];
  [super dealloc];
}

- (void)ensureContentsSizeDoesNotChange {
  NSView* contentsContainer = [self view];
  NSArray* subviews = [contentsContainer subviews];
  if ([subviews count] > 0)
    [contents_->GetNativeView() setAutoresizingMask:NSViewNotSizable];
}

// Call when the tab view is properly sized and the render widget host view
// should be put into the view hierarchy.
- (void)ensureContentsVisible {
  NSView* contentsContainer = [self view];
  NSArray* subviews = [contentsContainer subviews];
  NSView* contentsNativeView = contents_->GetNativeView();
  [contentsNativeView setFrame:[contentsContainer frame]];
  if ([subviews count] == 0) {
    [contentsContainer addSubview:contentsNativeView];
  } else if ([subviews objectAtIndex:0] != contentsNativeView) {
    [contentsContainer replaceSubview:[subviews objectAtIndex:0]
                                 with:contentsNativeView];
  }
  [contentsNativeView setAutoresizingMask:NSViewWidthSizable|
                                          NSViewHeightSizable];
}

// Returns YES if the tab represented by this controller is the front-most.
- (BOOL)isCurrentTab {
  // We're the current tab if we're in the view hierarchy, otherwise some other
  // tab is.
  return [[self view] superview] ? YES : NO;
}

- (void)willBecomeUnselectedTab {
  // The RWHV is ripped out of the view hierarchy on tab switches, so it never
  // formally resigns first responder status.  Handle this by explicitly sending
  // a Blur() message to the renderer, but only if the RWHV currently has focus.
  RenderViewHost* rvh = contents_->render_view_host();
  if (rvh && rvh->view() && rvh->view()->HasFocus())
    rvh->Blur();
}

- (void)willBecomeSelectedTab {
  // Do not explicitly call Focus() here, as the RWHV may not actually have
  // focus (for example, if the omnibox has focus instead).  The TabContents
  // logic will restore focus to the appropriate view.
}

- (void)tabDidChange:(TabContents*)updatedContents {
  // Calling setContentView: here removes any first responder status
  // the view may have, so avoid changing the view hierarchy unless
  // the view is different.
  if (contents_ != updatedContents) {
    contents_ = updatedContents;
    [self ensureContentsVisible];
  }
}

@end
