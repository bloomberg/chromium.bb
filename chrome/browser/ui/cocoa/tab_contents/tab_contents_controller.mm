// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"

#include <utility>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view.h"

using content::WebContents;
using content::WebContentsObserver;

// FullscreenObserver is used by TabContentsController to monitor for the
// showing/destruction of fullscreen render widgets.  When notified,
// TabContentsController will alter its child view hierarchy to either embed a
// fullscreen render widget view or restore the normal WebContentsView render
// view.  The embedded fullscreen render widget will fill the user's screen in
// the case where TabContentsController's NSView is a subview of a browser
// window that has been toggled into fullscreen mode (e.g., via
// FullscreenController).
class FullscreenObserver : public WebContentsObserver {
 public:
  explicit FullscreenObserver(TabContentsController* controller)
      : controller_(controller) {}

  void Observe(content::WebContents* new_web_contents) {
    WebContentsObserver::Observe(new_web_contents);
  }

  virtual void DidShowFullscreenWidget(int routing_id) OVERRIDE {
    [controller_ toggleFullscreenWidget:YES];
  }

  virtual void DidDestroyFullscreenWidget(int routing_id) OVERRIDE {
    [controller_ toggleFullscreenWidget:NO];
  }

 private:
  TabContentsController* const controller_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenObserver);
};

@implementation TabContentsController
@synthesize webContents = contents_;

- (id)initWithContents:(WebContents*)contents
    andAutoEmbedFullscreen:(BOOL)enableEmbeddedFullscreen {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    contents_ = contents;
    if (enableEmbeddedFullscreen) {
      fullscreenObserver_.reset(new FullscreenObserver(self));
      fullscreenObserver_->Observe(contents_);
    }
    isEmbeddingFullscreenWidget_ = NO;
  }
  return self;
}

- (void)dealloc {
  // make sure our contents have been removed from the window
  [[self view] removeFromSuperview];
  [super dealloc];
}

- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);
  [view setAutoresizingMask:NSViewHeightSizable|NSViewWidthSizable];
  [self setView:view];
}

- (void)ensureContentsSizeDoesNotChange {
  NSView* contentsContainer = [self view];
  NSArray* subviews = [contentsContainer subviews];
  if ([subviews count] > 0)
    [[subviews objectAtIndex:0] setAutoresizingMask:NSViewNotSizable];
}

// Call when the tab view is properly sized and the render widget host view
// should be put into the view hierarchy.
- (void)ensureContentsVisible {
  if (!contents_)
    return;
  NSView* contentsContainer = [self view];
  NSArray* subviews = [contentsContainer subviews];
  NSView* contentsNativeView;
  content::RenderWidgetHostView* const fullscreenView =
      isEmbeddingFullscreenWidget_ ?
      contents_->GetFullscreenRenderWidgetHostView() : NULL;
  if (fullscreenView) {
    contentsNativeView = fullscreenView->GetNativeView();
  } else {
    isEmbeddingFullscreenWidget_ = NO;
    contentsNativeView = contents_->GetView()->GetNativeView();
  }
  [contentsNativeView setFrame:[contentsContainer frame]];
  if ([subviews count] == 0) {
    [contentsContainer addSubview:contentsNativeView];
  } else if ([subviews objectAtIndex:0] != contentsNativeView) {
    [contentsContainer replaceSubview:[subviews objectAtIndex:0]
                                 with:contentsNativeView];
  }
  [contentsNativeView setAutoresizingMask:NSViewWidthSizable|
                                          NSViewHeightSizable];
  // The rendering path with overlapping views disabled causes bugs when
  // transitioning between composited and non-composited mode.
  // http://crbug.com/279472
  if (!fullscreenView)
    contents_->GetView()->SetAllowOverlappingViews(true);
}

- (void)changeWebContents:(WebContents*)newContents {
  contents_ = newContents;
  if (fullscreenObserver_) {
    fullscreenObserver_->Observe(contents_);
    isEmbeddingFullscreenWidget_ =
        contents_ && contents_->GetFullscreenRenderWidgetHostView();
  } else {
    isEmbeddingFullscreenWidget_ = NO;
  }
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
  if (rvh) {
    if (rvh->GetView() && rvh->GetView()->HasFocus()) {
      rvh->Blur();
      return;
    }
    DevToolsWindow* devtoolsWindow =
        DevToolsWindow::GetDockedInstanceForInspectedTab([self webContents]);
    if (devtoolsWindow) {
      content::RenderViewHost* devtoolsView =
          devtoolsWindow->web_contents()->GetRenderViewHost();
      if (devtoolsView && devtoolsView->GetView() &&
          devtoolsView->GetView()->HasFocus()) {
        devtoolsView->Blur();
      }
    }
  }
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
    [self changeWebContents:updatedContents];
    [self ensureContentsVisible];
  }
}

- (void)toggleFullscreenWidget:(BOOL)enterFullscreen {
  isEmbeddingFullscreenWidget_ = enterFullscreen &&
      contents_ && contents_->GetFullscreenRenderWidgetHostView();
  [self ensureContentsVisible];
}

@end
