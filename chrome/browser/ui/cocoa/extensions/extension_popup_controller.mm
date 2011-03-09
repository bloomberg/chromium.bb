// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"

#include <algorithm>

#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "content/common/notification_details.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_source.h"

namespace {
// The duration for any animations that might be invoked by this controller.
const NSTimeInterval kAnimationDuration = 0.2;

// There should only be one extension popup showing at one time. Keep a
// reference to it here.
static ExtensionPopupController* gPopup;

// Given a value and a rage, clamp the value into the range.
CGFloat Clamp(CGFloat value, CGFloat min, CGFloat max) {
  return std::max(min, std::min(max, value));
}

}  // namespace

class DevtoolsNotificationBridge : public NotificationObserver {
 public:
  explicit DevtoolsNotificationBridge(ExtensionPopupController* controller)
    : controller_(controller) {}

  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::EXTENSION_HOST_DID_STOP_LOADING: {
        if (Details<ExtensionHost>([controller_ extensionHost]) == details)
          [controller_ showDevTools];
        break;
      }
      case NotificationType::DEVTOOLS_WINDOW_CLOSING: {
        RenderViewHost* rvh = [controller_ extensionHost]->render_view_host();
        if (Details<RenderViewHost>(rvh) == details)
          // Allow the devtools to finish detaching before we close the popup
          [controller_ performSelector:@selector(close)
                            withObject:nil
                            afterDelay:0.0];
        break;
      }
      default: {
        NOTREACHED() << "Received unexpected notification";
        break;
      }
    };
  }

 private:
  ExtensionPopupController* controller_;
};

@interface ExtensionPopupController(Private)
// Callers should be using the public static method for initialization.
// NOTE: This takes ownership of |host|.
- (id)initWithHost:(ExtensionHost*)host
      parentWindow:(NSWindow*)parentWindow
        anchoredAt:(NSPoint)anchoredAt
     arrowLocation:(info_bubble::BubbleArrowLocation)arrowLocation
           devMode:(BOOL)devMode;

// Called when the extension's hosted NSView has been resized.
- (void)extensionViewFrameChanged;
@end

@implementation ExtensionPopupController

- (id)initWithHost:(ExtensionHost*)host
      parentWindow:(NSWindow*)parentWindow
        anchoredAt:(NSPoint)anchoredAt
     arrowLocation:(info_bubble::BubbleArrowLocation)arrowLocation
           devMode:(BOOL)devMode {

  parentWindow_ = parentWindow;
  anchor_ = [parentWindow convertBaseToScreen:anchoredAt];
  host_.reset(host);
  beingInspected_ = devMode;

  scoped_nsobject<InfoBubbleView> view([[InfoBubbleView alloc] init]);
  if (!view.get())
    return nil;
  [view setArrowLocation:arrowLocation];

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
  if (beingInspected_) {
    // Listen for the the devtools window closing.
    notificationBridge_.reset(new DevtoolsNotificationBridge(self));
    registrar_.reset(new NotificationRegistrar);
    registrar_->Add(notificationBridge_.get(),
                    NotificationType::DEVTOOLS_WINDOW_CLOSING,
                    Source<Profile>(host->profile()));
    registrar_->Add(notificationBridge_.get(),
                    NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
                    Source<Profile>(host->profile()));
  }
  return self;
}

- (void)showDevTools {
  DevToolsManager::GetInstance()->OpenDevToolsWindow(host_->render_view_host());
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
  if ([window isVisible] && !beingInspected_) {
    [self close];
  }
}

- (void)close {
  [parentWindow_ removeChildWindow:[self window]];

  // No longer have a parent window, so nil out the pointer and deregister for
  // notifications.
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center removeObserver:self
                    name:NSWindowWillCloseNotification
                  object:parentWindow_];
  parentWindow_ = nil;
  [super close];
}

- (BOOL)isClosing {
  return [static_cast<InfoBubbleWindow*>([self window]) isClosing];
}

- (ExtensionHost*)extensionHost {
  return host_.get();
}

+ (ExtensionPopupController*)showURL:(GURL)url
                           inBrowser:(Browser*)browser
                          anchoredAt:(NSPoint)anchoredAt
                       arrowLocation:(info_bubble::BubbleArrowLocation)
                                         arrowLocation
                             devMode:(BOOL)devMode {
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
     arrowLocation:arrowLocation
           devMode:devMode];
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
      Clamp(NSWidth(extensionFrame_),
            ExtensionViewMac::kMinWidth,
            ExtensionViewMac::kMaxWidth),
      Clamp(NSHeight(extensionFrame_),
            ExtensionViewMac::kMinHeight,
            ExtensionViewMac::kMaxHeight))];

  // Pad the window by half of the rounded corner radius to prevent the
  // extension's view from bleeding out over the corners.
  CGFloat inset = info_bubble::kBubbleCornerRadius / 2.0;
  [extensionView_ setFrameOrigin:NSMakePoint(inset, inset)];

  NSRect frame = [extensionView_ frame];
  frame.size.height += info_bubble::kBubbleArrowHeight +
                       info_bubble::kBubbleCornerRadius;
  frame.size.width += info_bubble::kBubbleCornerRadius;
  frame = [extensionView_ convertRectToBase:frame];
  // Adjust the origin according to the height and width so that the arrow is
  // positioned correctly at the middle and slightly down from the button.
  NSPoint windowOrigin = anchor_;
  NSSize offsets = NSMakeSize(info_bubble::kBubbleArrowXOffset +
                                  info_bubble::kBubbleArrowWidth / 2.0,
                              info_bubble::kBubbleArrowHeight / 2.0);
  offsets = [extensionView_ convertSize:offsets toView:nil];
  windowOrigin.x -= NSWidth(frame) - offsets.width;
  windowOrigin.y -= NSHeight(frame) - offsets.height;
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

- (void)windowDidResize:(NSNotification*)notification {
  // Let the extension view know, so that it can tell plugins.
  if (host_->view())
    host_->view()->WindowFrameChanged();
}

- (void)windowDidMove:(NSNotification*)notification {
  // Let the extension view know, so that it can tell plugins.
  if (host_->view())
    host_->view()->WindowFrameChanged();
}

// Private (TestingAPI)
- (NSView*)view {
  return extensionView_;
}

// Private (TestingAPI)
+ (NSSize)minPopupSize {
  NSSize minSize = {ExtensionViewMac::kMinWidth, ExtensionViewMac::kMinHeight};
  return minSize;
}

// Private (TestingAPI)
+ (NSSize)maxPopupSize {
  NSSize maxSize = {ExtensionViewMac::kMaxWidth, ExtensionViewMac::kMaxHeight};
  return maxSize;
}

@end
