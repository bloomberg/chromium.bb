// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/web_auth_flow_window_cocoa.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/browser_window_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#import "ui/base/cocoa/underlay_opengl_hosting_window.h"

// A window controller for a minimal window to host a web app view. Passes
// Objective-C notifications to the C++ bridge.
@interface WebAuthFlowWindowController
    : NSWindowController<NSWindowDelegate> {
 @private
  WebAuthFlowWindowCocoa* webAuthWindow_; // Weak; owns self.
}

@property(assign, nonatomic) WebAuthFlowWindowCocoa* webAuthWindow;

@end

@implementation WebAuthFlowWindowController

@synthesize webAuthWindow = webAuthWindow_;

- (void)windowWillClose:(NSNotification*)notification {
  if (webAuthWindow_)
    webAuthWindow_->WindowWillClose();
}

@end

WebAuthFlowWindowCocoa::WebAuthFlowWindowCocoa(
    Delegate* delegate,
    content::BrowserContext* browser_context,
    content::WebContents* contents)
    : WebAuthFlowWindow(delegate, browser_context, contents),
      attention_request_id_(0) {
}

void WebAuthFlowWindowCocoa::Show() {
  NSRect rect = NSMakeRect(0, 0, kDefaultWidth, kDefaultHeight);
  NSUInteger styleMask = NSTitledWindowMask | NSClosableWindowMask |
                         NSMiniaturizableWindowMask | NSResizableWindowMask;
  scoped_nsobject<NSWindow> window([[UnderlayOpenGLHostingWindow alloc]
      initWithContentRect:rect
                styleMask:styleMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);

  NSView* view = contents()->GetView()->GetNativeView();
  [view setFrame:rect];
  [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [[window contentView] addSubview:view];

  window_controller_.reset(
      [[WebAuthFlowWindowController alloc] initWithWindow:window.release()]);
  [[window_controller_ window] setDelegate:window_controller_];
  [window_controller_ setWebAuthWindow:this];

  [[window_controller_ window] center];
  [window_controller_ showWindow:nil];
}

void WebAuthFlowWindowCocoa::WindowWillClose() {
  if (delegate())
    delegate()->OnClose();
}

WebAuthFlowWindowCocoa::~WebAuthFlowWindowCocoa() {
  [window_controller_ setWebAuthWindow:NULL];
  [window_controller_ close];
}

// static
WebAuthFlowWindow* WebAuthFlowWindow::Create(
    Delegate* delegate,
    content::BrowserContext* browser_context,
    content::WebContents* contents) {
  return new WebAuthFlowWindowCocoa(delegate, browser_context, contents);
}
