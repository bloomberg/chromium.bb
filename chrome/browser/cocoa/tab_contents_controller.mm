// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_contents_controller.h"

#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/cocoa/view_id_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"

// Default offset of the contents splitter in pixels.
static const int kDefaultContentsSplitOffset = 400;

// Never make the web part of the tab contents smaller than this (needed if the
// window is only a few pixels high/wide).
const int kMinWebHeight = 50;
const int kMinWebWidth = 50;


@implementation TabContentsController

- (id)initWithNibName:(NSString*)name
             contents:(TabContents*)contents {
  if ((self = [super initWithNibName:name
                              bundle:mac_util::MainAppBundle()])) {
    contents_ = contents;
  }
  sidebarContents_ = NULL;
  return self;
}

- (void)dealloc {
  // make sure our contents have been removed from the window
  [[self view] removeFromSuperview];
  [super dealloc];
}

// Call when the tab view is properly sized and the render widget host view
// should be put into the view hierarchy.
- (void)ensureContentsVisible {
  NSArray* subviews = [contentsContainer_ subviews];
  if ([subviews count] == 0)
    [contentsContainer_ addSubview:contents_->GetNativeView()];
  else if ([subviews objectAtIndex:0] != contents_->GetNativeView())
    [contentsContainer_ replaceSubview:[subviews objectAtIndex:0]
                                  with:contents_->GetNativeView()];
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

- (void)showDevToolsContents:(TabContents*)devToolsContents {
  NSArray* subviews = [devToolsContainer_ subviews];
  if (devToolsContents) {
    DCHECK_GE([subviews count], 1u);

    // Load the default split offset.  If we are already showing devtools, we
    // will replace the default with the current devtools height.
    CGFloat splitOffset = g_browser_process->local_state()->GetInteger(
        prefs::kDevToolsSplitLocation);
    if (splitOffset == -1) {
      // Initial load, set to default value.
      splitOffset = kDefaultContentsSplitOffset;
    }

    // |devtoolsView| is a TabContentsViewCocoa object, whose ViewID was
    // set to VIEW_ID_TAB_CONTAINER initially, so we need to change it to
    // VIEW_ID_DEV_TOOLS_DOCKED here.
    NSView* devtoolsView = devToolsContents->GetNativeView();
    view_id_util::SetID(devtoolsView, VIEW_ID_DEV_TOOLS_DOCKED);
    if ([subviews count] == 1) {
      [devToolsContainer_ addSubview:devtoolsView];
    } else {
      DCHECK_EQ([subviews count], 2u);
      [devToolsContainer_ replaceSubview:[subviews objectAtIndex:1]
                                    with:devToolsContents->GetNativeView()];
      // If devtools are already visible, keep the current size.
      splitOffset = NSHeight([devtoolsView frame]);
    }

    // Make sure |splitOffset| isn't too large or too small.
    splitOffset = MIN(splitOffset,
                      NSHeight([devToolsContainer_ frame]) - kMinWebHeight);
    DCHECK_GE(splitOffset, 0) << "kMinWebHeight needs to be smaller than "
                              << "smallest available tab contents space.";
    splitOffset = MAX(0, splitOffset);

    // It seems as if |-setPosition:ofDividerAtIndex:| should do what's needed,
    // but I can't figure out how to use it. Manually resize web and devtools.
    NSRect devtoolsFrame = [devtoolsView frame];
    devtoolsFrame.size.height = splitOffset;
    [devtoolsView setFrame:devtoolsFrame];

    NSRect webFrame = [[subviews objectAtIndex:0] frame];
    webFrame.size.height = NSHeight([devToolsContainer_ frame]) -
                           [self devToolsHeight];
    [[subviews objectAtIndex:0] setFrame:webFrame];

    [devToolsContainer_ adjustSubviews];
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

- (CGFloat)devToolsHeight {
  NSArray* subviews = [devToolsContainer_ subviews];
  if ([subviews count] < 2)
    return 0;
  return NSHeight([[subviews objectAtIndex:1] frame]) +
         [devToolsContainer_ dividerThickness];
}

// This function is very similar to showDevToolsContents.
// TODO(alekseys): refactor and move both to browser window.
// I (alekseys) intend to do it very soon.
- (void)showSidebarContents:(TabContents*)sidebarContents {
  sidebarContents_ = sidebarContents;
  NSArray* subviews = [contentsContainer_ subviews];
  if (sidebarContents) {
    DCHECK_GE([subviews count], 1u);

    // Load the default split offset.
    CGFloat sidebarWidth = g_browser_process->local_state()->GetInteger(
        prefs::kExtensionSidebarWidth);
    if (sidebarWidth == -1) {
      // Initial load, set to default value.
      sidebarWidth = NSWidth([contentsContainer_ frame]) / 7;
    }

    // |sidebarView| is a TabContentsViewCocoa object, whose ViewID was
    // set to VIEW_ID_TAB_CONTAINER initially, so we need to change it to
    // VIEW_ID_SIDE_BAR_CONTAINER here.
    NSView* sidebarView = sidebarContents->GetNativeView();
    view_id_util::SetID(sidebarView, VIEW_ID_SIDE_BAR_CONTAINER);
    if ([subviews count] == 1) {
      [contentsContainer_ addSubview:sidebarView];
    } else {
      DCHECK_EQ([subviews count], 2u);
      sidebarWidth = NSWidth([[subviews objectAtIndex:1] frame]);
      [contentsContainer_ replaceSubview:[subviews objectAtIndex:1]
                                    with:sidebarContents->GetNativeView()];
    }

    // Make sure |sidebarWidth| isn't too large or too small.
    sidebarWidth = MIN(sidebarWidth,
                       NSWidth([contentsContainer_ frame]) - kMinWebWidth);
    DCHECK_GE(sidebarWidth, 0) << "kMinWebWidth needs to be smaller than "
                               << "smallest available tab contents space.";
    sidebarWidth = MAX(0, sidebarWidth);

    // It seems as if |-setPosition:ofDividerAtIndex:| should do what's needed,
    // but I can't figure out how to use it. Manually resize web and sidebar.
    NSRect sidebarFrame = [sidebarView frame];
    sidebarFrame.size.width = sidebarWidth;
    [sidebarView setFrame:sidebarFrame];

    NSRect webFrame = [[subviews objectAtIndex:0] frame];
    webFrame.size.width = NSWidth([contentsContainer_ frame]) -
    ([contentsContainer_ dividerThickness] + sidebarWidth);
    [[subviews objectAtIndex:0] setFrame:webFrame];

    [contentsContainer_ adjustSubviews];
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

- (TabContents*)sidebarContents {
  return sidebarContents_;
}

@end
