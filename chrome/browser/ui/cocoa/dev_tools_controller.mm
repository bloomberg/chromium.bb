// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/dev_tools_controller.h"

#include <algorithm>

#include <Cocoa/Cocoa.h>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace {

// Minimal height of devtools pane or content pane when devtools are docked
// to the browser window.
const int kMinDevToolsHeight = 50;
const int kMinDevToolsWidth = 150;
const int kMinContentsSize = 50;

}  // end namespace


@interface GraySplitView : NSSplitView
- (NSColor*)dividerColor;
@end


@implementation GraySplitView
- (NSColor*)dividerColor {
  return [NSColor darkGrayColor];
}
@end


@interface DevToolsController (Private)
- (void)showDevToolsContainer:(NSView*)container
                     dockSide:(DevToolsDockSide)dockSide
                      profile:(Profile*)profile;
- (void)hideDevToolsContainer:(Profile*)profile;
- (void)setDockToRight:(BOOL)dock_to_right
           withProfile:(Profile*)profile;
- (void)resizeDevTools:(CGFloat)size;
@end


@implementation DevToolsController

- (id)init {
  if ((self = [super init])) {
    splitView_.reset([[GraySplitView alloc] initWithFrame:NSZeroRect]);
    [splitView_ setDividerStyle:NSSplitViewDividerStyleThin];
    [splitView_ setVertical:NO];
    [splitView_ setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    [splitView_ setDelegate:self];

    dockSide_ = DEVTOOLS_DOCK_SIDE_BOTTOM;
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

- (void)updateDevToolsForWebContents:(WebContents*)contents
                         withProfile:(Profile*)profile {
  // Get current devtools content.
  DevToolsWindow* devToolsWindow = contents ?
      DevToolsWindow::GetDockedInstanceForInspectedTab(contents) : NULL;
  WebContents* devToolsContents = devToolsWindow ?
      devToolsWindow->tab_contents()->web_contents() : NULL;

  if (devToolsContents && devToolsContents->GetNativeView() &&
      [devToolsContents->GetNativeView() superview] == splitView_.get()) {
    [self setDockSide:devToolsWindow->dock_side() withProfile:profile];
    return;
  }

  NSArray* subviews = [splitView_ subviews];
  if (devToolsContents) {
    // |devToolsView| is a TabContentsViewCocoa object, whose ViewID was
    // set to VIEW_ID_TAB_CONTAINER initially, so we need to change it to
    // VIEW_ID_DEV_TOOLS_DOCKED here.
    NSView* devToolsView = devToolsContents->GetNativeView();
    view_id_util::SetID(devToolsView, VIEW_ID_DEV_TOOLS_DOCKED);
    [self showDevToolsContainer:devToolsView
                       dockSide:devToolsWindow->dock_side()
                        profile:profile];
  } else if ([subviews count] > 1) {
    [self hideDevToolsContainer:profile];
  }
}

- (void)setDockSide:(DevToolsDockSide)dockSide
        withProfile:(Profile*)profile {
  if (dockSide_ == dockSide)
    return;

  NSArray* subviews = [splitView_ subviews];
  if ([subviews count] == 2) {
    scoped_nsobject<NSView> devToolsContentsView(
        [[subviews objectAtIndex:1] retain]);
    [self hideDevToolsContainer:profile];
    dockSide_ = dockSide;
    [self showDevToolsContainer:devToolsContentsView
                       dockSide:dockSide
                        profile:profile];
  } else {
    dockSide_ = dockSide;
  }
}

- (void)showDevToolsContainer:(NSView*)container
                     dockSide:(BOOL)dockSide
                      profile:(Profile*)profile {
  NSArray* subviews = [splitView_ subviews];
  DCHECK_GE([subviews count], 1u);

  CGFloat splitOffset = 0;

  BOOL dockToRight = dockSide_ == DEVTOOLS_DOCK_SIDE_RIGHT;
  CGFloat contentSize =
      dockToRight ? NSWidth([splitView_ frame])
                  : NSHeight([splitView_ frame]);

  if ([subviews count] == 1) {
    // Load the default split offset.
    splitOffset = profile->GetPrefs()->
        GetInteger(dockToRight ? prefs::kDevToolsVSplitLocation :
                                 prefs::kDevToolsHSplitLocation);

    if (splitOffset < 0)
      splitOffset = contentSize * 1 / 3;

    [splitView_ addSubview:container];
  } else {
    DCHECK_EQ([subviews count], 2u);
    // If devtools are already visible, keep the current size.
    splitOffset = dockToRight ? NSWidth([[subviews objectAtIndex:1] frame])
                              : NSHeight([[subviews objectAtIndex:1] frame]);
    [splitView_ replaceSubview:[subviews objectAtIndex:1]
                          with:container];
  }

  // Make sure |splitOffset| isn't too large or too small.
  CGFloat minSize = dockToRight ? kMinDevToolsWidth: kMinDevToolsHeight;
  splitOffset = std::max(minSize, splitOffset);
  splitOffset = std::min(static_cast<CGFloat>(contentSize - kMinContentsSize),
                         splitOffset);

  if (splitOffset < 0)
    splitOffset = contentSize * 1 / 3;

  DCHECK_GE(splitOffset, 0) << "kMinWebHeight needs to be smaller than "
                            << "smallest available tab contents space.";

  [splitView_ setVertical: dockToRight];
  [self resizeDevTools:splitOffset];
}

- (void)hideDevToolsContainer:(Profile*)profile {
  NSArray* subviews = [splitView_ subviews];
  NSView* oldDevToolsContentsView = [subviews objectAtIndex:1];

  BOOL dockToRight = dockSide_ == DEVTOOLS_DOCK_SIDE_RIGHT;
  // Store split offset when hiding devtools window only.
  int splitOffset = dockToRight ? NSWidth([oldDevToolsContentsView frame])
                                : NSHeight([oldDevToolsContentsView frame]);
  profile->GetPrefs()->SetInteger(
      dockToRight ? prefs::kDevToolsVSplitLocation :
                    prefs::kDevToolsHSplitLocation,
      splitOffset);

  [oldDevToolsContentsView removeFromSuperview];
  [splitView_ adjustSubviews];
}

- (void)resizeDevTools:(CGFloat)size {
  NSArray* subviews = [splitView_ subviews];

  // It seems as if |-setPosition:ofDividerAtIndex:| should do what's needed,
  // but I can't figure out how to use it. Manually resize web and devtools.
  // TODO(alekseys): either make setPosition:ofDividerAtIndex: work or to add a
  // category on NSSplitView to handle manual resizing.
  NSView* webView = [subviews objectAtIndex:0];
  NSRect webFrame = [webView frame];
  NSView* devToolsView = [subviews objectAtIndex:1];
  NSRect devToolsFrame = [devToolsView frame];

  BOOL dockToRight = dockSide_ == DEVTOOLS_DOCK_SIDE_RIGHT;
  if (dockToRight)
    devToolsFrame.size.width = size;
  else
    devToolsFrame.size.height = size;

  if (dockToRight) {
    webFrame.size.width =
        NSWidth([splitView_ frame]) - ([splitView_ dividerThickness] + size);
  } else {
    webFrame.size.height =
        NSHeight([splitView_ frame]) - ([splitView_ dividerThickness] + size);
  }

  [webView setFrame:webFrame];
  [devToolsView setFrame:devToolsFrame];

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

-(void)splitViewWillResizeSubviews:(NSNotification *)notification {
  [[splitView_ window] disableScreenUpdatesUntilFlush];
}

@end
