// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/shell_window_cocoa.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/browser_window_utils.h"
#include "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#import "ui/base/cocoa/underlay_opengl_hosting_window.h"

@implementation ShellWindowController

@synthesize shellWindow = shellWindow_;

- (void)windowWillClose:(NSNotification*)notification {
  if (shellWindow_)
    shellWindow_->WindowWillClose();
}

@end

ShellWindowCocoa::ShellWindowCocoa(Profile* profile,
                                   const extensions::Extension* extension,
                                   const GURL& url,
                                   const ShellWindow::CreateParams params)
    : ShellWindow(profile, extension, url),
      attention_request_id_(0) {
  NSRect rect = NSMakeRect(params.bounds.x(), params.bounds.y(),
                           params.bounds.width(), params.bounds.height());
  NSUInteger styleMask = NSTitledWindowMask | NSClosableWindowMask |
                         NSMiniaturizableWindowMask | NSResizableWindowMask;
  scoped_nsobject<NSWindow> window([[UnderlayOpenGLHostingWindow alloc]
      initWithContentRect:rect
                styleMask:styleMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [window setTitle:base::SysUTF8ToNSString(extension->name())];

  NSView* view = web_contents()->GetView()->GetNativeView();
  [view setFrame:[[window contentView] bounds]];
  [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [[window contentView] addSubview:view];

  window_controller_.reset(
      [[ShellWindowController alloc] initWithWindow:window.release()]);
  [[window_controller_ window] setDelegate:window_controller_];
  [window_controller_ setShellWindow:this];
}

bool ShellWindowCocoa::IsActive() const {
  return [window() isKeyWindow];
}

bool ShellWindowCocoa::IsMaximized() const {
  return [window() isZoomed];
}

bool ShellWindowCocoa::IsMinimized() const {
  return [window() isMiniaturized];
}

bool ShellWindowCocoa::IsFullscreen() const {
  return false;
}

gfx::NativeWindow ShellWindowCocoa::GetNativeWindow() {
  return window();
}

gfx::Rect ShellWindowCocoa::GetRestoredBounds() const {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  NSRect frame = [window() frame];
  gfx::Rect bounds(frame.origin.x, 0, NSWidth(frame), NSHeight(frame));
  bounds.set_y(NSHeight([screen frame]) - NSMaxY(frame));
  return bounds;
}

gfx::Rect ShellWindowCocoa::GetBounds() const {
  return GetRestoredBounds();
}

void ShellWindowCocoa::Show() {
  [window_controller_ showWindow:nil];
  [window() makeKeyAndOrderFront:window_controller_];
}

void ShellWindowCocoa::ShowInactive() {
  [window() orderFront:window_controller_];
}

void ShellWindowCocoa::Close() {
  [window() performClose:nil];
}

void ShellWindowCocoa::Activate() {
  [BrowserWindowUtils activateWindowForController:window_controller_];
}

void ShellWindowCocoa::Deactivate() {
  // TODO(jcivelli): http://crbug.com/51364 Implement me.
  NOTIMPLEMENTED();
}

void ShellWindowCocoa::Maximize() {
  // Zoom toggles so only call if not already maximized.
  if (!IsMaximized())
    [window() zoom:window_controller_];
}

void ShellWindowCocoa::Minimize() {
  [window() miniaturize:window_controller_];
}

void ShellWindowCocoa::Restore() {
  if (IsMaximized())
    [window() zoom:window_controller_];  // Toggles zoom mode.
  else if (IsMinimized())
    [window() deminiaturize:window_controller_];
}

void ShellWindowCocoa::SetBounds(const gfx::Rect& bounds) {
  // Enforce minimum bounds.
  gfx::Rect checked_bounds = bounds;

  NSSize min_size = [window() minSize];
  if (bounds.width() < min_size.width)
      checked_bounds.set_width(min_size.width);
  if (bounds.height() < min_size.height)
      checked_bounds.set_height(min_size.height);

  NSRect cocoa_bounds = NSMakeRect(checked_bounds.x(), 0,
                                   checked_bounds.width(),
                                   checked_bounds.height());
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  cocoa_bounds.origin.y = NSHeight([screen frame]) - checked_bounds.bottom();

  [window() setFrame:cocoa_bounds display:YES];
}

void ShellWindowCocoa::SetDraggableRegion(SkRegion* region) {
  // TODO: implement
}

void ShellWindowCocoa::FlashFrame(bool flash) {
  if (flash) {
    attention_request_id_ = [NSApp requestUserAttention:NSInformationalRequest];
  } else {
    [NSApp cancelUserAttentionRequest:attention_request_id_];
    attention_request_id_ = 0;
  }
}

bool ShellWindowCocoa::IsAlwaysOnTop() const {
  return false;
}

void ShellWindowCocoa::WindowWillClose() {
  [window_controller_ setShellWindow:NULL];
  delete this;
}

ShellWindowCocoa::~ShellWindowCocoa() {
}

NSWindow* ShellWindowCocoa::window() const {
  return [window_controller_ window];
}

// static
ShellWindow* ShellWindow::CreateImpl(Profile* profile,
                                     const extensions::Extension* extension,
                                     const GURL& url,
                                     const ShellWindow::CreateParams params) {
  return new ShellWindowCocoa(profile, extension, url, params);
}
