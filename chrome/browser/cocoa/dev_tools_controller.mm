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
@end


@implementation DevToolsController

- (id)initWithView:(NSSplitView*)devToolsView
          delegate:(id<DevToolsControllerDelegate>)delegate {
  DCHECK(delegate);
  if ((self = [super init])) {
    devToolsView_.reset([devToolsView retain]);
    delegate_ = delegate;
  }
  return self;
}

- (void)updateDevToolsForTabContents:(TabContents*)contents {
  // Get current devtools content.
  TabContents* devToolsContents = contents ?
      DevToolsWindow::GetDevToolsContents(contents) : NULL;

  [self showDevToolsContents:devToolsContents];
}

- (void)showDevToolsContents:(TabContents*)devToolsContents {
  NSArray* subviews = [devToolsView_ subviews];
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
      [devToolsView_ addSubview:devToolsView];
    } else {
      DCHECK_EQ([subviews count], 2u);
      // If devtools are already visible, keep the current size.
      splitOffset = NSHeight([devToolsView frame]);
      [devToolsView_ replaceSubview:[subviews objectAtIndex:1]
                               with:devToolsView];
    }

    // Make sure |splitOffset| isn't too large or too small.
    splitOffset =
        std::min(splitOffset, NSHeight([devToolsView_ frame]) - kMinWebHeight);
    DCHECK_GE(splitOffset, 0) << "kMinWebHeight needs to be smaller than "
                              << "smallest available tab contents space.";
    splitOffset = std::max(static_cast<CGFloat>(0), splitOffset);

    [delegate_ resizeDevToolsToNewHeight:splitOffset];
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

@end
