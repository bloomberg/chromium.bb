// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/dev_tools_controller.h"

#include <algorithm>
#include <cmath>

#include <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/base/cocoa/base_view.h"
#include "ui/base/cocoa/focus_tracker.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/mac/scoped_ns_disable_screen_updates.h"
#include "ui/gfx/size_conversions.h"

namespace {

bool CoreAnimationIsEnabled() {
  static bool is_enabled = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableCoreAnimation);
  return is_enabled;
}

}

using content::WebContents;

@interface DevToolsContainerView : BaseView {
  DevToolsContentsResizingStrategy strategy_;

  // Weak references. Ownership via -subviews.
  NSView* devToolsView_;
  NSView* contentsView_;
}

- (void)setContentsResizingStrategy:
    (const DevToolsContentsResizingStrategy&)strategy;
- (void)adjustSubviews;
- (void)showDevTools:(NSView*)devToolsView;
- (void)hideDevTools;

@end


@implementation DevToolsContainerView

- (void)setContentsResizingStrategy:
    (const DevToolsContentsResizingStrategy&)strategy {
  strategy_.CopyFrom(strategy);
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldBoundsSize {
  [self adjustSubviews];
}

- (void)showDevTools:(NSView*)devToolsView {
  NSArray* subviews = [self subviews];
  DCHECK_EQ(1u, [subviews count]);
  contentsView_ = [subviews objectAtIndex:0];
  devToolsView_ = devToolsView;
  if (CoreAnimationIsEnabled()) {
    // Make sure we do not draw any transient arrangements of views.
    gfx::ScopedNSDisableScreenUpdates disabler;
    [self replaceSubview:contentsView_ with:devToolsView_];
    [devToolsView_ addSubview:contentsView_];
  } else {
    // Place DevTools under contents.
    [self addSubview:devToolsView positioned:NSWindowBelow relativeTo:nil];
  }
}

- (void)hideDevTools {
  if (CoreAnimationIsEnabled()) {
    // Make sure we do not draw any transient arrangements of views.
    gfx::ScopedNSDisableScreenUpdates disabler;
    DCHECK_EQ(1u, [[self subviews] count]);
    [contentsView_ removeFromSuperview];
    [self replaceSubview:devToolsView_ with:contentsView_];
  } else {
    DCHECK_EQ(2u, [[self subviews] count]);
    [devToolsView_ removeFromSuperview];
  }
  contentsView_ = nil;
  devToolsView_ = nil;
}

- (void)adjustSubviews {
  if (![[self subviews] count])
    return;

  if (!devToolsView_) {
    DCHECK_EQ(1u, [[self subviews] count]);
    NSView* contents = [[self subviews] objectAtIndex:0];
    [contents setFrame:[self bounds]];
    return;
  }

  if (CoreAnimationIsEnabled()) {
    DCHECK_EQ(1u, [[self subviews] count]);
  } else {
    DCHECK_EQ(2u, [[self subviews] count]);
  }

  gfx::Rect new_devtools_bounds;
  gfx::Rect new_contents_bounds;
  ApplyDevToolsContentsResizingStrategy(
      strategy_, gfx::Size(NSSizeToCGSize([self bounds].size)),
      [self flipNSRectToRect:[devToolsView_ bounds]],
      [self flipNSRectToRect:[contentsView_ bounds]],
      &new_devtools_bounds, &new_contents_bounds);
  [devToolsView_ setFrame:[self flipRectToNSRect:new_devtools_bounds]];
  [contentsView_ setFrame:[self flipRectToNSRect:new_contents_bounds]];
}

@end

@interface DevToolsController (Private)
- (void)showDevToolsView;
- (void)hideDevToolsView;
@end


@implementation DevToolsController

- (id)init {
  if ((self = [super init])) {
    devToolsContainerView_.reset(
        [[DevToolsContainerView alloc] initWithFrame:NSZeroRect]);
    [devToolsContainerView_
        setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
  }
  return self;
}

- (NSView*)view {
  return devToolsContainerView_.get();
}

- (void)updateDevToolsForWebContents:(WebContents*)contents
                         withProfile:(Profile*)profile {
  DevToolsWindow* newDevToolsWindow = contents ?
      DevToolsWindow::GetDockedInstanceForInspectedTab(contents) : NULL;

  // Make sure we do not draw any transient arrangements of views.
  gfx::ScopedNSDisableScreenUpdates disabler;
  bool shouldHide = devToolsWindow_ && devToolsWindow_ != newDevToolsWindow;
  bool shouldShow = newDevToolsWindow && devToolsWindow_ != newDevToolsWindow;

  if (shouldHide)
    [self hideDevToolsView];

  devToolsWindow_ = newDevToolsWindow;
  if (devToolsWindow_) {
    const DevToolsContentsResizingStrategy& strategy =
        devToolsWindow_->GetContentsResizingStrategy();
    devToolsWindow_->web_contents()->GetView()->SetOverlayView(
        contents->GetView(),
        gfx::Point(strategy.insets().left(), strategy.insets().top()));
    [devToolsContainerView_ setContentsResizingStrategy:strategy];
  } else {
    DevToolsContentsResizingStrategy zeroStrategy;
    [devToolsContainerView_ setContentsResizingStrategy:zeroStrategy];
  }

  if (shouldShow)
    [self showDevToolsView];

  [devToolsContainerView_ adjustSubviews];
}

- (void)showDevToolsView {
  focusTracker_.reset(
      [[FocusTracker alloc] initWithWindow:[devToolsContainerView_ window]]);

  // |devToolsView| is a WebContentsViewCocoa object, whose ViewID was
  // set to VIEW_ID_TAB_CONTAINER initially, so we need to change it to
  // VIEW_ID_DEV_TOOLS_DOCKED here.
  NSView* devToolsView =
      devToolsWindow_->web_contents()->GetView()->GetNativeView();
  view_id_util::SetID(devToolsView, VIEW_ID_DEV_TOOLS_DOCKED);

  [devToolsContainerView_ showDevTools:devToolsView];
}

- (void)hideDevToolsView {
  devToolsWindow_->web_contents()->GetView()->RemoveOverlayView();
  [devToolsContainerView_ hideDevTools];
  [focusTracker_ restoreFocusInWindow:[devToolsContainerView_ window]];
  focusTracker_.reset();
}

@end
