// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"

#include <algorithm>

#include "base/callback.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "components/web_modal/popup_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/cocoa/window_size_constants.h"

using content::BrowserContext;
using content::RenderViewHost;
using content::WebContents;

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
- (id)initWithHost:(extensions::ExtensionViewHost*)host
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

// Called when the window moves or resizes. Notifies the extension.
- (void)onWindowChanged;

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
    : controller_(controller),
      web_contents_([controller_ extensionViewHost]->host_contents()),
      devtools_callback_(base::Bind(
          &DevtoolsNotificationBridge::OnDevToolsStateChanged,
          base::Unretained(this))) {
    content::DevToolsManager::GetInstance()->AddAgentStateCallback(
        devtools_callback_);
  }

  virtual ~DevtoolsNotificationBridge() {
    content::DevToolsManager::GetInstance()->RemoveAgentStateCallback(
        devtools_callback_);
  }

  void OnDevToolsStateChanged(content::DevToolsAgentHost* agent_host,
                              bool attached) {
    if (agent_host->GetWebContents() != web_contents_)
      return;

    if (attached) {
      // Set the flag on the controller so the popup is not hidden when
      // the dev tools get focus.
      [controller_ setBeingInspected:YES];
    } else {
      // Allow the devtools to finish detaching before we close the popup.
      [controller_ performSelector:@selector(close)
                        withObject:nil
                        afterDelay:0.0];
    }
  }

  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING: {
        if (content::Details<extensions::ExtensionViewHost>(
                [controller_ extensionViewHost]) == details) {
          [controller_ showDevTools];
        }
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
  // WebContents for controller. Hold onto this separately because we need to
  // know what it is for notifications, but our ExtensionViewHost may not be
  // valid.
  WebContents* web_contents_;
  base::Callback<void(content::DevToolsAgentHost*, bool)> devtools_callback_;
};

@implementation ExtensionPopupController

- (id)initWithHost:(extensions::ExtensionViewHost*)host
      parentWindow:(NSWindow*)parentWindow
        anchoredAt:(NSPoint)anchoredAt
     arrowLocation:(info_bubble::BubbleArrowLocation)arrowLocation
           devMode:(BOOL)devMode {
  base::scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
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
    ignoreWindowDidResignKey_ = NO;

    InfoBubbleView* view = self.bubble;
    [view setArrowLocation:arrowLocation];

    extensionView_ = host->view()->GetNativeView();
    container_.reset(new ExtensionPopupContainer(self));
    static_cast<ExtensionViewMac*>(host->view())
        ->set_container(container_.get());

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(extensionViewFrameChanged)
                   name:NSViewFrameDidChangeNotification
                 object:extensionView_];

    [view addSubview:extensionView_];

    notificationBridge_.reset(new DevtoolsNotificationBridge(self));
    registrar_.reset(new content::NotificationRegistrar);
    if (beingInspected_) {
      // Listen for the extension to finish loading so the dev tools can be
      // opened.
      registrar_->Add(notificationBridge_.get(),
                      extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                      content::Source<BrowserContext>(host->browser_context()));
    }
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)showDevTools {
  DevToolsWindow::OpenDevToolsWindow(host_->host_contents());
}

- (void)close {
  // |windowWillClose:| could have already been called. http://crbug.com/279505
  if (host_) {
    // TODO(gbillock): Change this API to say directly if the current popup
    // should block tab close? This is a bit over-reaching.
    web_modal::PopupManager* popup_manager =
        web_modal::PopupManager::FromWebContents(host_->host_contents());
    if (popup_manager && popup_manager->IsWebModalDialogActive(
            host_->host_contents())) {
      return;
    }
  }
  [super close];
}

- (void)windowWillClose:(NSNotification *)notification {
  [super windowWillClose:notification];
  if (gPopup == self)
    gPopup = nil;
  if (host_->view())
    static_cast<ExtensionViewMac*>(host_->view())->set_container(NULL);
  host_.reset();
}

- (void)windowDidResignKey:(NSNotification*)notification {
  // |windowWillClose:| could have already been called. http://crbug.com/279505
  if (host_) {
    // When a modal dialog is opened on top of the popup and when it's closed,
    // it steals key-ness from the popup. Don't close the popup when this
    // happens. There's an extra windowDidResignKey: notification after the
    // modal dialog closes that should also be ignored.
    web_modal::PopupManager* popupManager =
        web_modal::PopupManager::FromWebContents(
            host_->host_contents());
    if (popupManager &&
        popupManager->IsWebModalDialogActive(host_->host_contents())) {
      ignoreWindowDidResignKey_ = YES;
      return;
    }
    if (ignoreWindowDidResignKey_) {
      ignoreWindowDidResignKey_ = NO;
      return;
    }
  }
  if (!beingInspected_)
    [super windowDidResignKey:notification];
}

- (BOOL)isClosing {
  return [static_cast<InfoBubbleWindow*>([self window]) isClosing];
}

- (extensions::ExtensionViewHost*)extensionViewHost {
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

  // If we click the browser/page action again, we should close the popup.
  // Make Mac behavior the same with Windows and others.
  if (gPopup) {
    std::string extension_id = url.host();
    if (url.SchemeIs(content::kChromeUIScheme) &&
        url.host() == chrome::kChromeUIExtensionInfoHost)
      extension_id = url.path().substr(1);
    extensions::ExtensionViewHost* host = [gPopup extensionViewHost];
    if (extension_id == host->extension_id()) {
      [gPopup close];
      return nil;
    }
  }

  extensions::ExtensionViewHost* host =
      extensions::ExtensionViewHostFactory::CreatePopupHost(url, browser);
  DCHECK(host);
  if (!host)
    return nil;

  [gPopup close];

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
  id animator = [window animator];
  if ([window isVisible] &&
      ([animator alphaValue] < 1.0 ||
       !NSEqualRects([window frame], [animator frame]))) {
    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext] setDuration:kAnimationDuration];
    [animator setAlphaValue:1.0];
    [animator setFrame:frame display:YES];
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

- (void)onWindowChanged {
  ExtensionViewMac* extensionView =
      static_cast<ExtensionViewMac*>(host_->view());
  // Let the extension view know, so that it can tell plugins.
  if (extensionView)
    extensionView->WindowFrameChanged();
}

- (void)windowDidResize:(NSNotification*)notification {
  [self onWindowChanged];
}

- (void)windowDidMove:(NSNotification*)notification {
  [self onWindowChanged];
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
