// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/sidebar_controller.h"

#include <algorithm>

#include <Cocoa/Cocoa.h>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace {

// By default sidebar width is 1/7th of the current page content width.
const CGFloat kDefaultSidebarWidthRatio = 1.0 / 7;

// Never make the web part of the tab contents smaller than this (needed if the
// window is only a few pixels wide).
const int kMinWebWidth = 50;

}  // end namespace


@interface SidebarController (Private)
- (void)showSidebarContents:(TabContents*)sidebarContents;
- (void)resizeSidebarToNewWidth:(CGFloat)width;
@end


@implementation SidebarController

- (id)initWithDelegate:(id<TabContentsControllerDelegate>)delegate {
  if ((self = [super init])) {
    splitView_.reset([[NSSplitView alloc] initWithFrame:NSZeroRect]);
    [splitView_ setDividerStyle:NSSplitViewDividerStyleThin];
    [splitView_ setVertical:YES];
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

- (NSSplitView*)view {
  return splitView_.get();
}

- (NSSplitView*)splitView {
  return splitView_.get();
}

- (void)updateSidebarForTabContents:(TabContents*)contents {
  // Get the active sidebar content.
  if (SidebarManager::GetInstance() == NULL)  // Happens in tests.
    return;

  TabContents* sidebarContents = NULL;
  if (contents && SidebarManager::IsSidebarAllowed()) {
    SidebarContainer* activeSidebar =
        SidebarManager::GetInstance()->GetActiveSidebarContainerFor(contents);
    if (activeSidebar)
      sidebarContents = activeSidebar->sidebar_contents();
  }

  TabContents* oldSidebarContents = [contentsController_ tabContents];
  if (oldSidebarContents == sidebarContents)
    return;

  // Adjust sidebar view.
  [self showSidebarContents:sidebarContents];

  // Notify extensions.
  SidebarManager::GetInstance()->NotifyStateChanges(
      oldSidebarContents, sidebarContents);
}

- (void)ensureContentsVisible {
  [contentsController_ ensureContentsVisible];
}

- (void)showSidebarContents:(TabContents*)sidebarContents {
  [contentsController_ ensureContentsSizeDoesNotChange];

  NSArray* subviews = [splitView_ subviews];
  if (sidebarContents) {
    DCHECK_GE([subviews count], 1u);

    // Native view is a TabContentsViewCocoa object, whose ViewID was
    // set to VIEW_ID_TAB_CONTAINER initially, so change it to
    // VIEW_ID_SIDE_BAR_CONTAINER here.
    view_id_util::SetID(
        sidebarContents->GetNativeView(), VIEW_ID_SIDE_BAR_CONTAINER);

    CGFloat sidebarWidth = 0;
    if ([subviews count] == 1) {
      // Load the default split offset.
      sidebarWidth = g_browser_process->local_state()->GetInteger(
          prefs::kExtensionSidebarWidth);
      if (sidebarWidth < 0) {
        // Initial load, set to default value.
        sidebarWidth =
            NSWidth([splitView_ frame]) * kDefaultSidebarWidthRatio;
      }
      [splitView_ addSubview:[contentsController_ view]];
    } else {
      DCHECK_EQ([subviews count], 2u);
      sidebarWidth = NSWidth([[subviews objectAtIndex:1] frame]);
    }

    // Make sure |sidebarWidth| isn't too large or too small.
    sidebarWidth = std::min(sidebarWidth,
                            NSWidth([splitView_ frame]) - kMinWebWidth);
    DCHECK_GE(sidebarWidth, 0) << "kMinWebWidth needs to be smaller than "
                               << "smallest available tab contents space.";
    sidebarWidth = std::max(static_cast<CGFloat>(0), sidebarWidth);

    [self resizeSidebarToNewWidth:sidebarWidth];
  } else {
    if ([subviews count] > 1) {
      NSView* oldSidebarContentsView = [subviews objectAtIndex:1];
      // Store split offset when hiding sidebar window only.
      int sidebarWidth = NSWidth([oldSidebarContentsView frame]);
      g_browser_process->local_state()->SetInteger(
          prefs::kExtensionSidebarWidth, sidebarWidth);
      [oldSidebarContentsView removeFromSuperview];
      [splitView_ adjustSubviews];
    }
  }

  [contentsController_ changeTabContents:sidebarContents];
}

- (void)resizeSidebarToNewWidth:(CGFloat)width {
  NSArray* subviews = [splitView_ subviews];

  // It seems as if |-setPosition:ofDividerAtIndex:| should do what's needed,
  // but I can't figure out how to use it. Manually resize web and sidebar.
  // TODO(alekseys): either make setPosition:ofDividerAtIndex: work or to add a
  // category on NSSplitView to handle manual resizing.
  NSView* sidebarView = [subviews objectAtIndex:1];
  NSRect sidebarFrame = [sidebarView frame];
  sidebarFrame.size.width = width;
  [sidebarView setFrame:sidebarFrame];

  NSView* webView = [subviews objectAtIndex:0];
  NSRect webFrame = [webView frame];
  webFrame.size.width =
      NSWidth([splitView_ frame]) - ([splitView_ dividerThickness] + width);
  [webView setFrame:webFrame];

  [splitView_ adjustSubviews];
}

// NSSplitViewDelegate protocol.
- (BOOL)splitView:(NSSplitView *)splitView
    shouldAdjustSizeOfSubview:(NSView *)subview {
  // Return NO for the sidebar view to indicate that it should not be resized
  // automatically.  The sidebar keeps the width set by the user.
  if ([[splitView_ subviews] indexOfObject:subview] == 1)
    return NO;
  return YES;
}

@end
