// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/dev_tools_controller.h"

#include <algorithm>

#include <Cocoa/Cocoa.h>

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#import "chrome/browser/cocoa/view_id_util.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"

namespace {

// Default offset of the contents splitter in pixels.
const int kDefaultContentsSplitOffset = 400;

// Never make the web part of the tab contents smaller than this (needed if the
// window is only a few pixels high).
const int kMinWebHeight = 50;

}  // end namespace


@interface DevToolsController (Private)
- (void)showDevToolsContents:(TabContents*)devToolsContents;
- (void)resizeDevToolsToNewHeight:(CGFloat)height;
@end


@implementation DevToolsController

- (id)init {
  if ((self = [super init])) {
    splitView_.reset([[NSSplitView alloc] initWithFrame:NSZeroRect]);
    [splitView_ setDividerStyle:NSSplitViewDividerStyleThin];
    [splitView_ setVertical:NO];
    [splitView_ setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
  }
  return self;
}

- (NSView*)view {
  return splitView_.get();
}

- (NSSplitView*)splitView {
  return splitView_.get();
}

- (void)updateDevToolsForTabContents:(TabContents*)contents {
  // Get current devtools content.
  TabContents* devToolsContents = contents ?
      DevToolsWindow::GetDevToolsContents(contents) : NULL;

  [self showDevToolsContents:devToolsContents];
}

- (void)showDevToolsContents:(TabContents*)devToolsContents {
  NSArray* subviews = [splitView_ subviews];
  if (devToolsContents) {
    DCHECK_GE([subviews count], 1u);

    // |devToolsView| is a TabContentsViewCocoa object, whose ViewID was
    // set to VIEW_ID_TAB_CONTAINER initially, so we need to change it to
    // VIEW_ID_DEV_TOOLS_DOCKED here.
    NSView* devToolsView = devToolsContents->GetNativeView();
    view_id_util::SetID(devToolsView, VIEW_ID_DEV_TOOLS_DOCKED);

    CGFloat splitOffset = 0;
    if ([subviews count] == 1) {
      // Load the default split offset.
      splitOffset = g_browser_process->local_state()->GetInteger(
          prefs::kDevToolsSplitLocation);
      if (splitOffset < 0) {
        // Initial load, set to default value.
        splitOffset = kDefaultContentsSplitOffset;
      }
      [splitView_ addSubview:devToolsView];
    } else {
      DCHECK_EQ([subviews count], 2u);
      // If devtools are already visible, keep the current size.
      splitOffset = NSHeight([devToolsView frame]);
      [splitView_ replaceSubview:[subviews objectAtIndex:1]
                            with:devToolsView];
    }

    // Make sure |splitOffset| isn't too large or too small.
    splitOffset =
        std::min(splitOffset, NSHeight([splitView_ frame]) - kMinWebHeight);
    DCHECK_GE(splitOffset, 0) << "kMinWebHeight needs to be smaller than "
                              << "smallest available tab contents space.";
    splitOffset = std::max(static_cast<CGFloat>(0), splitOffset);

    [self resizeDevToolsToNewHeight:splitOffset];
  } else {
    if ([subviews count] > 1) {
      NSView* oldDevToolsContentsView = [subviews objectAtIndex:1];
      // Store split offset when hiding devtools window only.
      int splitOffset = NSHeight([oldDevToolsContentsView frame]);
      g_browser_process->local_state()->SetInteger(
          prefs::kDevToolsSplitLocation, splitOffset);
      [oldDevToolsContentsView removeFromSuperview];
    }
  }
}

- (void)resizeDevToolsToNewHeight:(CGFloat)height {
  NSArray* subviews = [splitView_ subviews];

  // It seems as if |-setPosition:ofDividerAtIndex:| should do what's needed,
  // but I can't figure out how to use it. Manually resize web and devtools.
  // TODO(alekseys): either make setPosition:ofDividerAtIndex: work or to add a
  // category on NSSplitView to handle manual resizing.
  NSView* devToolsView = [subviews objectAtIndex:1];
  NSRect devToolsFrame = [devToolsView frame];
  devToolsFrame.size.height = height;
  [devToolsView setFrame:devToolsFrame];

  NSView* webView = [subviews objectAtIndex:0];
  NSRect webFrame = [webView frame];
  webFrame.size.height =
      NSHeight([splitView_ frame]) - ([splitView_ dividerThickness] + height);
  [webView setFrame:webFrame];

  [splitView_ adjustSubviews];
}



@end
