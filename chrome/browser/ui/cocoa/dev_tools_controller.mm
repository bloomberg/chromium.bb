// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/dev_tools_controller.h"

#include <algorithm>

#include <Cocoa/Cocoa.h>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace {

// Default offset of the contents splitter in pixels.
const int kDefaultContentsSplitOffset = 400;

// Never make the web part of the tab contents smaller than this (needed if the
// window is only a few pixels high).
const int kMinWebHeight = 50;

}  // end namespace


@interface DevToolsController (Private)
- (void)showDevToolsContents:(TabContents*)devToolsContents
                 withProfile:(Profile*)profile;
- (void)resizeDevToolsToNewHeight:(CGFloat)height;
@end


@implementation DevToolsController

- (id)initWithDelegate:(id<TabContentsControllerDelegate>)delegate {
  if ((self = [super init])) {
    splitView_.reset([[NSSplitView alloc] initWithFrame:NSZeroRect]);
    [splitView_ setDividerStyle:NSSplitViewDividerStyleThin];
    [splitView_ setVertical:NO];
    [splitView_ setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    [splitView_ setDelegate:self];

    contentsController_.reset(
        [[TabContentsController alloc] initWithContents:NULL
                                               delegate:delegate]);
  }
  return self;
}

- (void)dealloc {
  [splitView_ setDelegate:nil];
  [super dealloc];
}

- (NSView*)view {
  return splitView_.get();
}

- (NSSplitView*)splitView {
  return splitView_.get();
}

- (void)updateDevToolsForTabContents:(TabContents*)contents
                         withProfile:(Profile*)profile {
  // Get current devtools content.
  TabContentsWrapper* devToolsTab = contents ?
      DevToolsWindow::GetDevToolsContents(contents) : NULL;
  TabContents* devToolsContents = devToolsTab ?
      devToolsTab->tab_contents() : NULL;

  [self showDevToolsContents:devToolsContents withProfile:profile];
}

- (void)ensureContentsVisible {
  [contentsController_ ensureContentsVisible];
}

- (void)showDevToolsContents:(TabContents*)devToolsContents
                 withProfile:(Profile*)profile {
  [contentsController_ ensureContentsSizeDoesNotChange];

  NSArray* subviews = [splitView_ subviews];
  if (devToolsContents) {
    DCHECK_GE([subviews count], 1u);

    // |devToolsView| is a TabContentsViewCocoa object, whose ViewID was
    // set to VIEW_ID_TAB_CONTAINER initially, so we need to change it to
    // VIEW_ID_DEV_TOOLS_DOCKED here.
    view_id_util::SetID(
        devToolsContents->GetNativeView(), VIEW_ID_DEV_TOOLS_DOCKED);

    CGFloat splitOffset = 0;
    if ([subviews count] == 1) {
      // Load the default split offset.
      splitOffset = profile->GetPrefs()->
          GetInteger(prefs::kDevToolsSplitLocation);
      if (splitOffset < 0) {
        // Initial load, set to default value.
        splitOffset = kDefaultContentsSplitOffset;
      }
      [splitView_ addSubview:[contentsController_ view]];
    } else {
      DCHECK_EQ([subviews count], 2u);
      // If devtools are already visible, keep the current size.
      splitOffset = NSHeight([[subviews objectAtIndex:1] frame]);
    }

    // Make sure |splitOffset| isn't too large or too small.
    splitOffset = std::max(static_cast<CGFloat>(kMinWebHeight), splitOffset);
    splitOffset =
        std::min(splitOffset, NSHeight([splitView_ frame]) - kMinWebHeight);
    DCHECK_GE(splitOffset, 0) << "kMinWebHeight needs to be smaller than "
                              << "smallest available tab contents space.";

    [self resizeDevToolsToNewHeight:splitOffset];
  } else {
    if ([subviews count] > 1) {
      NSView* oldDevToolsContentsView = [subviews objectAtIndex:1];
      // Store split offset when hiding devtools window only.
      int splitOffset = NSHeight([oldDevToolsContentsView frame]);

      profile->GetPrefs()->SetInteger(
          prefs::kDevToolsSplitLocation, splitOffset);
      [oldDevToolsContentsView removeFromSuperview];
      [splitView_ adjustSubviews];
    }
  }

  [contentsController_ changeTabContents:devToolsContents];
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

// NSSplitViewDelegate protocol.
- (BOOL)splitView:(NSSplitView *)splitView
    shouldAdjustSizeOfSubview:(NSView *)subview {
  // Return NO for the devTools view to indicate that it should not be resized
  // automatically. It preserves the height set by the user and also keeps
  // view height the same while changing tabs when one of the tabs shows infobar
  // and others are not.
  if ([[splitView_ subviews] indexOfObject:subview] == 1)
    return NO;
  return YES;
}

@end
