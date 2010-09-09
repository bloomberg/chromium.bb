// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/sidebar_controller.h"

#include <algorithm>

#include <Cocoa/Cocoa.h>

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#import "chrome/browser/cocoa/view_id_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"

namespace {

// By default sidebar width is 1/7th of the current page content width.
const CGFloat kDefaultSidebarWidthRatio = 1.0 / 7;

// Never make the web part of the tab contents smaller than this (needed if the
// window is only a few pixels wide).
const int kMinWebWidth = 50;

}  // end namespace


@interface SidebarController (Private)
- (void)showSidebarContents:(TabContents*)sidebarContents;
@end


@implementation SidebarController

- (id)initWithView:(NSSplitView*)sidebarView
          delegate:(id<SidebarControllerDelegate>)delegate {
  DCHECK(delegate);
  if ((self = [super init])) {
    sidebarView_.reset([sidebarView retain]);
    delegate_ = delegate;
    sidebarContents_ = NULL;
  }
  return self;
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
  if (sidebarContents_ == sidebarContents)
    return;

  TabContents* oldSidebarContents = sidebarContents_;
  sidebarContents_ = sidebarContents;

  // Adjust sidebar view.
  [self showSidebarContents:sidebarContents];

  // Notify extensions.
  SidebarManager::GetInstance()->NotifyStateChanges(
      oldSidebarContents, sidebarContents);
}

- (void)showSidebarContents:(TabContents*)sidebarContents {
  NSArray* subviews = [sidebarView_ subviews];
  if (sidebarContents) {
    DCHECK_GE([subviews count], 1u);

    // |sidebarView| is a TabContentsViewCocoa object, whose ViewID was
    // set to VIEW_ID_TAB_CONTAINER initially, so we need to change it to
    // VIEW_ID_SIDE_BAR_CONTAINER here.
    NSView* sidebarView = sidebarContents->GetNativeView();
    view_id_util::SetID(sidebarView, VIEW_ID_SIDE_BAR_CONTAINER);

    CGFloat sidebarWidth = 0;
    if ([subviews count] == 1) {
      // Load the default split offset.
      sidebarWidth = g_browser_process->local_state()->GetInteger(
          prefs::kExtensionSidebarWidth);
      if (sidebarWidth < 0) {
        // Initial load, set to default value.
        sidebarWidth =
            NSWidth([sidebarView_ frame]) * kDefaultSidebarWidthRatio;
      }
      [sidebarView_ addSubview:sidebarView];
    } else {
      DCHECK_EQ([subviews count], 2u);
      sidebarWidth = NSWidth([[subviews objectAtIndex:1] frame]);
      [sidebarView_ replaceSubview:[subviews objectAtIndex:1]
                              with:sidebarView];
    }

    // Make sure |sidebarWidth| isn't too large or too small.
    sidebarWidth = std::min(sidebarWidth,
                            NSWidth([sidebarView_ frame]) - kMinWebWidth);
    DCHECK_GE(sidebarWidth, 0) << "kMinWebWidth needs to be smaller than "
                               << "smallest available tab contents space.";
    sidebarWidth = std::max(static_cast<CGFloat>(0), sidebarWidth);

    [delegate_ resizeSidebarToNewWidth:sidebarWidth];
  } else {
    if ([subviews count] > 1) {
      NSView* oldSidebarContentsView = [subviews objectAtIndex:1];
      // Store split offset when hiding sidebar window only.
      int sidebarWidth = NSWidth([oldSidebarContentsView frame]);
      g_browser_process->local_state()->SetInteger(
          prefs::kExtensionSidebarWidth, sidebarWidth);
      [oldSidebarContentsView removeFromSuperview];
    }
  }
}

@end
