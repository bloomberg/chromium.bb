// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"

#include <algorithm>

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/cocoa/window_size_constants.h"

using content::RenderViewHost;

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

@interface ExtensionPopupController (Private)
// Callers should be using the public static method for initialization.
// NOTE: This takes ownership of |host|.
- (id)initWithHost:(extensions::ExtensionHost*)host
      parentWindow:(NSWindow*)parentWindow
        anchoredAt:(NSPoint)anchoredAt
     arrowLocation:(info_bubble::BubbleArrowLocation)arrowLocation
           devMode:(BOOL)devMode;

// Called when the extension's hosted NSView has been resized.
- (void)extensionViewFrameChanged;

// Called when the extension's size changes.
- (void)onSizeChanged:(NSSize)newSize;

// Called when the extension view is shown.
- (void)onViewDidShow;
@end

class ExtensionPopupContainer : public ExtensionViewMac::Container {
 public:
  explicit ExtensionPopupContainer(ExtensionPopupController* controller)
      : controller_(controller) {
  }

  virtual void OnExtensionSizeChanged(
      ExtensionViewMac* view,
      const gfx::Size& new_size) OVERRIDE {
    [controller_ onSizeChanged:
        NSMakeSize(new_size.width(), new_size.height())];
  }

  virtual void OnExtensionViewDidShow(ExtensionViewMac* view) OVERRIDE {
    [controller_ onViewDidShow];
  }

 private:
  ExtensionPopupController* controller_; // Weak; owns this.
};

class DevtoolsNotificationBridge : public content::NotificationObserver {
 public:
  explicit DevtoolsNotificationBridge(ExtensionPopupController* controller)
    : controller_(controller) {}

  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING: {
        if (content::Details<extensions::ExtensionHost>(
                [controller_ extensionHost]) == details) {
          [controller_ showDevTools];
        }
        break;
      }
      case content::NOTIFICATION_DEVTOOLS_AGENT_ATTACHED: {
        RenderViewHost* rvh = [controller_ extensionHost]->render_view_host();
        if (content::Details<RenderViewHost>(rvh) == details)
          // Set the flag on the controller so the popup is not hidden when
          // the dev tools get focus.
          [controller_ setBeingInspected:YES];
        break;
      }
      case content::NOTIFICATION_DEVTOOLS_AGENT_DETACHED: {
        RenderViewHost* rvh = [controller_ extensionHost]->render_view_host();
        if (content::Details<RenderViewHost>(rvh) == details)
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

@implementation ExtensionPopupController

- (id)initWithHost:(extensions::ExtensionHost*)host
      parentWindow:(NSWindow*)parentWindow
        anchoredAt:(NSPoint)anchoredAt
     arrowLocation:(info_bubble::BubbleArrowLocation)arrowLocation
           devMode:(BOOL)devMode {
  scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater
                    styleMask:NSBorderlessWindowMask
                      backing:NSBackingStoreBuffered
                        defer:YES]);
  if (!window.get())
    return nil;

  anchoredAt = [parentWindow convertBaseToScreen:anchoredAt];
  if ((self = [super initWithWindow:window
                       parentWindow:parentWindow
                         anchoredAt:anchoredAt])) {
    host_.reset(host);
    beingInspected_ = devMode;

    InfoBubbleView* view = self.bubble;
    [view setArrowLocation:arrowLocation];

    extensionView_ = host->view()->native_view();
    container_.reset(new ExtensionPopupContainer(self));
    host->view()->set_container(container_.get());

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(extensionViewFrameChanged)
                   name:NSViewFrameDidChangeNotification
                 object:extensionView_];

    [view addSubview:extensionView_];

    notificationBridge_.reset(new DevtoolsNotificationBridge(self));
    registrar_.reset(new content::NotificationRegistrar);
    // Listen for the the devtools window closing so we can close this window if
    // it is being inspected and the inspector is closed.
    registrar_->Add(notificationBridge_.get(),
                    content::NOTIFICATION_DEVTOOLS_AGENT_DETACHED,
                    content::Source<content::BrowserContext>(
                        host->profile()));
    if (beingInspected_) {
      // Listen for the extension to finish loading so the dev tools can be
      // opened.
      registrar_->Add(notificationBridge_.get(),
                      chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                      content::Source<Profile>(host->profile()));
    } else {
      // Listen for the dev tools opening on this popup, so we can stop it going
      // away when the dev tools get focus.
      registrar_->Add(notificationBridge_.get(),
                      content::NOTIFICATION_DEVTOOLS_AGENT_ATTACHED,
                      content::Source<content::BrowserContext>(
                          host->profile()));
    }
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)showDevTools {
  DevToolsWindow::OpenDevToolsWindow(host_->render_view_host());
}

- (void)windowWillClose:(NSNotification *)notification {
  [super windowWillClose:notification];
  gPopup = nil;
  if (host_->view())
    host_->view()->set_container(NULL);
  host_.reset();
}

- (void)windowDidResignKey:(NSNotification*)notification {
  if (!beingInspected_)
    [super windowDidResignKey:notification];
}

- (BOOL)isClosing {
  return [static_cast<InfoBubbleWindow*>([self window]) isClosing];
}

- (extensions::ExtensionHost*)extensionHost {
  return host_.get();
}

- (void)setBeingInspected:(BOOL)beingInspected {
  beingInspected_ = beingInspected;
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
      extensions::ExtensionSystem::Get(browser->profile())->process_manager();
  DCHECK(manager);
  if (!manager)
    return nil;

  extensions::ExtensionHost* host = manager->CreatePopupHost(url, browser);
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
      parentWindow:browser->window()->GetNativeWindow()
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
  frame = [extensionView_ convertRect:frame toView:nil];
  // Adjust the origin according to the height and width so that the arrow is
  // positioned correctly at the middle and slightly down from the button.
  NSPoint windowOrigin = self.anchorPoint;
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

- (void)onSizeChanged:(NSSize)newSize {
  // When we update the size, the window will become visible. Stay hidden until
  // the host is loaded.
  pendingSize_ = newSize;
  if (!host_->did_stop_loading())
    return;

  // No need to use CA here, our caller calls us repeatedly to animate the
  // resizing.
  NSRect frame = [extensionView_ frame];
  frame.size = newSize;

  // |new_size| is in pixels. Convert to view units.
  frame.size = [extensionView_ convertSize:frame.size fromView:nil];

  [extensionView_ setFrame:frame];
  [extensionView_ setNeedsDisplay:YES];
}

- (void)onViewDidShow {
  [self onSizeChanged:pendingSize_];
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
