// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/shell_window_cocoa.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"
#include "chrome/common/extensions/extension.h"

@implementation ShellWindowController

@synthesize shellWindow = shellWindow_;

- (void)windowWillClose:(NSNotification*)notification {
  if (shellWindow_)
    shellWindow_->WindowWillClose();
}

@end

ShellWindowCocoa::ShellWindowCocoa(ExtensionHost* host) : ShellWindow(host) {
  // TOOD(mihaip): Allow window dimensions to be specified in manifest (and
  // restore prior window dimensions and positions on relaunch).
  NSRect rect = NSMakeRect(0, 0, 512, 384);
  NSUInteger styleMask = NSTitledWindowMask | NSClosableWindowMask |
                         NSMiniaturizableWindowMask | NSResizableWindowMask;
  scoped_nsobject<NSWindow> window(
      [[NSWindow alloc] initWithContentRect:rect
                                  styleMask:styleMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  [window setTitle:base::SysUTF8ToNSString(host->extension()->name())];

  NSView* view = host->view()->native_view();
  [view setFrame:rect];
  [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [[window contentView] addSubview:view];

  window_controller_.reset(
      [[ShellWindowController alloc] initWithWindow:window.release()]);
  [[window_controller_ window] setDelegate:window_controller_];
  [window_controller_ setShellWindow:this];
  [window_controller_ showWindow:nil];
}

void ShellWindowCocoa::Close() {
  [[window_controller_ window] performClose:nil];
}

void ShellWindowCocoa::WindowWillClose() {
  [window_controller_ setShellWindow:NULL];
  delete this;
}

ShellWindowCocoa::~ShellWindowCocoa() {
}

// static
ShellWindow* ShellWindow::CreateShellWindow(ExtensionHost* host) {
  return new ShellWindowCocoa(host);
}
