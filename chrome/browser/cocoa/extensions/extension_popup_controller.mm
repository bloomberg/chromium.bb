// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/extensions/extension_popup_controller.h"

#include <algorithm>

#include "chrome/browser/browser.h"
#import "chrome/browser/cocoa/browser_window_cocoa.h"
#import "chrome/browser/cocoa/extension_view_mac.h"
#import "chrome/browser/cocoa/info_bubble_window.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"

// The minimum/maximum dimensions of the popup.
// The minimum is just a little larger than the size of the button itself.
// The maximum is an arbitrary number that should be smaller than most screens.
namespace {
const CGFloat kMinWidth = 25;
const CGFloat kMinHeight = 25;
const CGFloat kMaxWidth = 800;
const CGFloat kMaxHeight = 600;

// The duration for any animations that might be invoked by this controller.
const NSTimeInterval kAnimationDuration = 0.2;

// There should only be one extension popup showing at one time. Keep a
// reference to it here.
static ExtensionPopupController* gPopup;
}  // namespace

@interface ExtensionPopupController(Private)
// Callers should be using the public static method for initialization.
// NOTE: This takes ownership of |host|.
- (id)initWithHost:(ExtensionHost*)host
      parentWindow:(NSWindow*)parentWindow
        anchoredAt:(NSPoint)anchoredAt
     arrowLocation:(BubbleArrowLocation)arrowLocation;

// Called when the extension's hosted NSView has been resized.
- (void)extensionViewFrameChanged;
@end

@implementation ExtensionPopupController

- (id)initWithHost:(ExtensionHost*)host
      parentWindow:(NSWindow*)parentWindow
        anchoredAt:(NSPoint)anchoredAt
     arrowLocation:(BubbleArrowLocation)arrowLocation {
  parentWindow_ = parentWindow;
  anchor_ = anchoredAt;
  host_.reset(host);

  scoped_nsobject<InfoBubbleView> view([[InfoBubbleView alloc] init]);
  if (!view.get())
    return nil;
  [view setArrowLocation:arrowLocation];
  [view setBubbleType:kWhiteInfoBubble];

  host->view()->set_is_toolstrip(NO);

  extensionView_ = host->view()->native_view();
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(extensionViewFrameChanged)
                 name:NSViewFrameDidChangeNotification
               object:extensionView_];

  // Watch to see if the parent window closes, and if so, close this one.
  [center addObserver:self
             selector:@selector(parentWindowWillClose:)
                 name:NSWindowWillCloseNotification
               object:parentWindow_];

  [view addSubview:extensionView_];
  scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc]
          initWithContentRect:NSZeroRect
                    styleMask:NSBorderlessWindowMask
                      backing:NSBackingStoreBuffered
                        defer:YES]);
  if (!window.get())
    return nil;

  [window setDelegate:self];
  [window setContentView:view];
  self = [super initWithWindow:window];

  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)parentWindowWillClose:(NSNotification*)notification {
  [self close];
}

- (void)windowWillClose:(NSNotification *)notification {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [gPopup autorelease];
  gPopup = nil;
}

- (void)windowDidResignKey:(NSNotification *)notification {
  NSWindow* window = [self window];
  DCHECK_EQ([notification object], window);
  // If the window isn't visible, it is already closed, and this notification
  // has been sent as part of the closing operation, so no need to close.
  if ([window isVisible]) {
    [self close];
  }
}

- (void)close {
  [parentWindow_ removeChildWindow:[self window]];
  [super close];
}

- (BOOL)isClosing {
  return [static_cast<InfoBubbleWindow*>([self window]) isClosing];
}

+ (ExtensionPopupController*)showURL:(GURL)url
                           inBrowser:(Browser*)browser
                          anchoredAt:(NSPoint)anchoredAt
                       arrowLocation:(BubbleArrowLocation)arrowLocation {
  DCHECK([NSThread isMainThread]);
  DCHECK(browser);
  if (!browser)
    return nil;

  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  DCHECK(manager);
  if (!manager)
    return nil;

  ExtensionHost* host = manager->CreatePopup(url, browser);
  DCHECK(host);
  if (!host)
    return nil;

  // Make absolutely sure that no popups are leaked.
  if (gPopup) {
    if ([[gPopup window] isVisible])
      [gPopup close];

    [gPopup autorelease];
    gPopup = nil;
  }
  DCHECK(!gPopup);

  // Takes ownership of |host|. Also will autorelease itself when the popup is
  // closed, so no need to do that here.
  gPopup = [[ExtensionPopupController alloc]
      initWithHost:host
      parentWindow:browser->window()->GetNativeHandle()
        anchoredAt:anchoredAt
     arrowLocation:arrowLocation];

  return gPopup;
}

+ (ExtensionPopupController*)popup {
  return gPopup;
}

- (void)extensionViewFrameChanged {
  // If there are no changes in the width or height of the frame, then ignore.
  if (NSEqualSizes([extensionView_ frame].size, extensionFrame_.size))
    return;

  extensionFrame_ = [extensionView_ frame];
  // Constrain the size of the view.
  [extensionView_ setFrameSize:NSMakeSize(
      std::max(kMinWidth, std::min(kMaxWidth, NSWidth(extensionFrame_))),
      std::max(kMinHeight, std::min(kMaxHeight, NSHeight(extensionFrame_))))];

  // Pad the window by half of the rounded corner radius to prevent the
  // extension's view from bleeding out over the corners.
  CGFloat inset = kBubbleCornerRadius / 2.0;
  [extensionView_ setFrameOrigin:NSMakePoint(inset, inset)];

  NSRect frame = [extensionView_ frame];
  frame.size.height += kBubbleArrowHeight + kBubbleCornerRadius;
  frame.size.width += kBubbleCornerRadius;
  // Adjust the origin according to the height and width so that the arrow is
  // positioned correctly at the middle and slightly down from the button.
  NSPoint windowOrigin = anchor_;
  windowOrigin.x -= NSWidth(frame) - kBubbleArrowXOffset -
      (kBubbleArrowWidth / 2.0);
  windowOrigin.y -= NSHeight(frame) - (kBubbleArrowHeight / 2.0);
  frame.origin = windowOrigin;

  // Is the window still animating in? If so, then cancel that and create a new
  // animation setting the opacity and new frame value. Otherwise the current
  // animation will continue after this frame is set, reverting the frame to
  // what it was when the animation started.
  NSWindow* window = [self window];
  if ([window isVisible] && [[window animator] alphaValue] < 1.0) {
    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext] setDuration:kAnimationDuration];
    [[window animator] setAlphaValue:1.0];
    [[window animator] setFrame:frame display:YES];
    [NSAnimationContext endGrouping];
  } else {
    [window setFrame:frame display:YES];
  }

  // A NSViewFrameDidChangeNotification won't be sent until the extension view
  // content is loaded. The window is hidden on init, so show it the first time
  // the notification is fired (and consequently the view contents have loaded).
  if (![window isVisible]) {
    [self showWindow:self];
  }
}

// We want this to be a child of a browser window. addChildWindow: (called from
// this function) will bring the window on-screen; unfortunately,
// [NSWindowController showWindow:] will also bring it on-screen (but will cause
// unexpected changes to the window's position). We cannot have an
// addChildWindow: and a subsequent showWindow:. Thus, we have our own version.
- (void)showWindow:(id)sender {
  [parentWindow_ addChildWindow:[self window] ordered:NSWindowAbove];
  [[self window] makeKeyAndOrderFront:self];
}

// Private (TestingAPI)
- (NSView*)view {
  return extensionView_;
}

// Private (TestingAPI)
+ (NSSize)minPopupSize {
  NSSize minSize = {kMinWidth, kMinHeight};
  return minSize;
}

// Private (TestingAPI)
+ (NSSize)maxPopupSize {
  NSSize maxSize = {kMaxWidth, kMaxHeight};
  return maxSize;
}

@end
