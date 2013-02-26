// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"

#include <utility>

#include "base/memory/scoped_nsobject.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

using content::WebContents;


@implementation TabContentsController
@synthesize webContents = contents_;

- (id)initWithContents:(WebContents*)contents {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    contents_ = contents;
  }
  return self;
}

- (void)dealloc {
  // make sure our contents have been removed from the window
  [[self view] removeFromSuperview];
  [super dealloc];
}

- (void)loadView {
  scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);
  [view setAutoresizingMask:NSViewHeightSizable|NSViewWidthSizable];
  [self setView:view];
}

- (void)ensureContentsSizeDoesNotChange {
  NSView* contentsContainer = [self view];
  NSArray* subviews = [contentsContainer subviews];
  if ([subviews count] > 0) {
    [contents_->GetView()->GetNativeView()
        setAutoresizingMask:NSViewNotSizable];
  }
}

// Call when the tab view is properly sized and the render widget host view
// should be put into the view hierarchy.
- (void)ensureContentsVisible {
  if (!contents_)
    return;
  NSView* contentsContainer = [self view];
  NSArray* subviews = [contentsContainer subviews];
  NSView* contentsNativeView = contents_->GetView()->GetNativeView();
  [contentsNativeView setFrame:[contentsContainer frame]];
  if ([subviews count] == 0) {
    [contentsContainer addSubview:contentsNativeView];
  } else if ([subviews objectAtIndex:0] != contentsNativeView) {
    [contentsContainer replaceSubview:[subviews objectAtIndex:0]
                                 with:contentsNativeView];
  }
  [contentsNativeView setAutoresizingMask:NSViewWidthSizable|
                                          NSViewHeightSizable];
  // The find bar will overlap the content's view when it comes out; inform
  // the view.
  contents_->GetView()->SetAllowOverlappingViews(true);
}

- (void)changeWebContents:(WebContents*)newContents {
  contents_ = newContents;
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
  content::RenderViewHost* rvh = [self webContents]->GetRenderViewHost();
  if (rvh && rvh->GetView() && rvh->GetView()->HasFocus())
    rvh->Blur();
}

- (void)willBecomeSelectedTab {
  // Do not explicitly call Focus() here, as the RWHV may not actually have
  // focus (for example, if the omnibox has focus instead).  The WebContents
  // logic will restore focus to the appropriate view.
}

- (void)tabDidChange:(WebContents*)updatedContents {
  // Calling setContentView: here removes any first responder status
  // the view may have, so avoid changing the view hierarchy unless
  // the view is different.
  if ([self webContents] != updatedContents) {
    contents_ = updatedContents;
    [self ensureContentsVisible];
  }
}

@end
