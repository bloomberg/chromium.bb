// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/extensions/extension_popup_controller.h"

#include <algorithm>

#include "chrome/browser/browser.h"
#import "chrome/browser/cocoa/browser_window_cocoa.h"
#import "chrome/browser/cocoa/extension_view_mac.h"
#import "chrome/browser/cocoa/info_bubble_window.h"
#include "chrome/browser/profile.h"

// The minimum/maximum dimensions of the popup.
// The minimum is just a little larger than the size of the button itself.
// The maximum is an arbitrary number that should be smaller than most screens.
namespace {
const CGFloat kMinWidth = 25;
const CGFloat kMinHeight = 25;
const CGFloat kMaxWidth = 800;
const CGFloat kMaxHeight = 600;
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
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(extensionViewFrameChanged)
             name:NSViewFrameDidChangeNotification
           object:extensionView_];

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

- (void)windowWillClose:(NSNotification *)notification {
  [self autorelease];
}

- (ExtensionHost*)host {
  return host_.get();
}

- (void)windowDidResignKey:(NSNotification *)notification {
  DCHECK_EQ([notification object], [self window]);
  [self close];
}

- (void)close {
  [parentWindow_ removeChildWindow:[self window]];
  [super close];
}

+ (ExtensionPopupController*)showURL:(GURL)url
                           inBrowser:(Browser*)browser
                          anchoredAt:(NSPoint)anchoredAt
                       arrowLocation:(BubbleArrowLocation)arrowLocation {
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

  // Takes ownership of |host|. Also will autorelease itself when the popup is
  // closed, so no need to do that here.
  ExtensionPopupController* popup = [[ExtensionPopupController alloc]
      initWithHost:host
      parentWindow:browser->window()->GetNativeHandle()
        anchoredAt:anchoredAt
     arrowLocation:arrowLocation];

  return popup;
}

- (void)extensionViewFrameChanged {
  // Constrain the size of the view.
  [extensionView_ setFrameSize:NSMakeSize(
      std::max(kMinWidth, std::min(kMaxWidth, NSWidth([extensionView_ frame]))),
      std::max(kMinHeight,
          std::min(kMaxHeight, NSHeight([extensionView_ frame]))))];

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
  [[self window] setFrame:frame display:YES];

  // A NSViewFrameDidChangeNotification won't be sent until the extension view
  // content is loaded. The window is hidden on init, so show it the first time
  // the notification is fired (and consequently the view contents have loaded).
  //
  // TODO(andybons): It seems that if the frame changes again before the
  // animation of the window show is completed, the window size gets super
  // janky. Fix this.
  if (![[self window] isVisible]) {
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

@end
