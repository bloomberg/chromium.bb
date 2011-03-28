// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"

#include "base/memory/scoped_nsobject.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"

@interface TabContentsController(Private)
// Forwards frame update to |delegate_| (ResizeNotificationView calls it).
- (void)tabContentsViewFrameWillChange:(NSRect)frameRect;
// Notification from TabContents (forwarded by TabContentsNotificationBridge).
- (void)tabContentsRenderViewHostChanged:(RenderViewHost*)oldHost
                                 newHost:(RenderViewHost*)newHost;
@end


// A supporting C++ bridge object to register for TabContents notifications.

class TabContentsNotificationBridge : public NotificationObserver {
 public:
  explicit TabContentsNotificationBridge(TabContentsController* controller);

  // Overriden from NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
  // Register for |contents|'s notifications, remove all prior registrations.
  void ChangeTabContents(TabContents* contents);
 private:
  NotificationRegistrar registrar_;
  TabContentsController* controller_;  // weak, owns us
};

TabContentsNotificationBridge::TabContentsNotificationBridge(
    TabContentsController* controller)
    : controller_(controller) {
}

void TabContentsNotificationBridge::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (type == NotificationType::RENDER_VIEW_HOST_CHANGED) {
    RenderViewHostSwitchedDetails* switched_details =
        Details<RenderViewHostSwitchedDetails>(details).ptr();
    [controller_ tabContentsRenderViewHostChanged:switched_details->old_host
                                          newHost:switched_details->new_host];
  } else {
    NOTREACHED();
  }
}

void TabContentsNotificationBridge::ChangeTabContents(TabContents* contents) {
  registrar_.RemoveAll();
  if (contents) {
    registrar_.Add(this,
                   NotificationType::RENDER_VIEW_HOST_CHANGED,
                   Source<NavigationController>(&contents->controller()));
  }
}


// A custom view that notifies |controller| that view's frame is changing.

@interface ResizeNotificationView : NSView {
  TabContentsController* controller_;
}
- (id)initWithController:(TabContentsController*)controller;
@end

@implementation ResizeNotificationView

- (id)initWithController:(TabContentsController*)controller {
  if ((self = [super initWithFrame:NSZeroRect])) {
    controller_ = controller;
  }
  return self;
}

- (void)setFrame:(NSRect)frameRect {
  [controller_ tabContentsViewFrameWillChange:frameRect];
  [super setFrame:frameRect];
}

@end


@implementation TabContentsController
@synthesize tabContents = contents_;

- (id)initWithContents:(TabContents*)contents
              delegate:(id<TabContentsControllerDelegate>)delegate {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    contents_ = contents;
    delegate_ = delegate;
    tabContentsBridge_.reset(new TabContentsNotificationBridge(self));
    tabContentsBridge_->ChangeTabContents(contents);
  }
  return self;
}

- (void)dealloc {
  // make sure our contents have been removed from the window
  [[self view] removeFromSuperview];
  [super dealloc];
}

- (void)loadView {
  scoped_nsobject<ResizeNotificationView> view(
      [[ResizeNotificationView alloc] initWithController:self]);
  [view setAutoresizingMask:NSViewHeightSizable|NSViewWidthSizable];
  [self setView:view];
}

- (void)ensureContentsSizeDoesNotChange {
  if (contents_) {
    NSView* contentsContainer = [self view];
    NSArray* subviews = [contentsContainer subviews];
    if ([subviews count] > 0)
      [contents_->GetNativeView() setAutoresizingMask:NSViewNotSizable];
  }
}

// Call when the tab view is properly sized and the render widget host view
// should be put into the view hierarchy.
- (void)ensureContentsVisible {
  if (!contents_)
    return;
  NSView* contentsContainer = [self view];
  NSArray* subviews = [contentsContainer subviews];
  NSView* contentsNativeView = contents_->GetNativeView();

  NSRect contentsNativeViewFrame = [contentsContainer frame];
  contentsNativeViewFrame.origin = NSZeroPoint;

  [delegate_ tabContentsViewFrameWillChange:self
                                  frameRect:contentsNativeViewFrame];

  // Native view is resized to the actual size before it becomes visible
  // to avoid flickering.
  [contentsNativeView setFrame:contentsNativeViewFrame];
  if ([subviews count] == 0) {
    [contentsContainer addSubview:contentsNativeView];
  } else if ([subviews objectAtIndex:0] != contentsNativeView) {
    [contentsContainer replaceSubview:[subviews objectAtIndex:0]
                                 with:contentsNativeView];
  }
  // Restore autoresizing properties possibly stripped by
  // ensureContentsSizeDoesNotChange call.
  [contentsNativeView setAutoresizingMask:NSViewWidthSizable|
                                          NSViewHeightSizable];
}

- (void)changeTabContents:(TabContents*)newContents {
  contents_ = newContents;
  tabContentsBridge_->ChangeTabContents(contents_);
}

- (void)tabContentsViewFrameWillChange:(NSRect)frameRect {
  [delegate_ tabContentsViewFrameWillChange:self frameRect:frameRect];
}

- (void)tabContentsRenderViewHostChanged:(RenderViewHost*)oldHost
                                 newHost:(RenderViewHost*)newHost {
  if (oldHost && newHost && oldHost->view() && newHost->view()) {
    newHost->view()->set_reserved_contents_rect(
        oldHost->view()->reserved_contents_rect());
  } else {
    [delegate_ tabContentsViewFrameWillChange:self
                                    frameRect:[[self view] frame]];
  }
}

- (void)willBecomeUnselectedTab {
  // The RWHV is ripped out of the view hierarchy on tab switches, so it never
  // formally resigns first responder status.  Handle this by explicitly sending
  // a Blur() message to the renderer, but only if the RWHV currently has focus.
  RenderViewHost* rvh = [self tabContents]->render_view_host();
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
  if ([self tabContents] != updatedContents) {
    [self changeTabContents:updatedContents];
    if ([self tabContents])
      [self ensureContentsVisible];
  }
}

@end
