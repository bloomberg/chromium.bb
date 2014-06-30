// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/plugin/plugin_interpose_util_mac.h"

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include "content/child/npapi/webplugin_delegate_impl.h"
#include "content/common/plugin_process_messages.h"
#include "content/plugin/plugin_thread.h"

using content::PluginThread;

namespace {

// Brings the plugin process to the front so that the user can see its windows.
void SwitchToPluginProcess() {
  ProcessSerialNumber this_process, front_process;
  if ((GetCurrentProcess(&this_process) != noErr) ||
      (GetFrontProcess(&front_process) != noErr)) {
    return;
  }

  Boolean matched = false;
  if ((SameProcess(&this_process, &front_process, &matched) == noErr) &&
      !matched) {
    SetFrontProcess(&this_process);
  }
}

// Sends a message to the browser process to inform it that the given window
// has been shown.
void NotifyBrowserOfPluginShowWindow(uint32 window_id, CGRect bounds,
                                     bool modal) {
  PluginThread* plugin_thread = PluginThread::current();
  if (plugin_thread) {
    gfx::Rect window_bounds(bounds);
    plugin_thread->Send(
        new PluginProcessHostMsg_PluginShowWindow(window_id, window_bounds,
                                                  modal));
  }
}

// Sends a message to the browser process to inform it that the given window
// has been hidden, and switches focus back to the browser process if there are
// no remaining plugin windows.
void NotifyBrowserOfPluginHideWindow(uint32 window_id, CGRect bounds) {
  PluginThread* plugin_thread = PluginThread::current();
  if (plugin_thread) {
    gfx::Rect window_bounds(bounds);
    plugin_thread->Send(
        new PluginProcessHostMsg_PluginHideWindow(window_id, window_bounds));
  }
}

// Informs the host that the plugin has changed the cursor visibility.
void NotifyPluginOfSetCursorVisibility(bool visibility) {
  PluginThread* plugin_thread = PluginThread::current();
  if (plugin_thread) {
    plugin_thread->Send(
        new PluginProcessHostMsg_PluginSetCursorVisibility(visibility));
  }
}

struct WindowInfo {
  uint32 window_id;
  CGRect bounds;
  WindowInfo(NSWindow* window) {
    NSInteger window_num = [window windowNumber];
    window_id = window_num > 0 ? window_num : 0;
    bounds = NSRectToCGRect([window frame]);
  }
};

void OnPluginWindowClosed(const WindowInfo& window_info) {
  if (window_info.window_id == 0)
    return;
  NotifyBrowserOfPluginHideWindow(window_info.window_id, window_info.bounds);
}

BOOL g_waiting_for_window_number = NO;

void OnPluginWindowShown(const WindowInfo& window_info, BOOL is_modal) {
  // The window id is 0 if it has never been shown (including while it is the
  // process of being shown for the first time); when that happens, we'll catch
  // it in _setWindowNumber instead.
  static BOOL s_pending_display_is_modal = NO;
  if (window_info.window_id == 0) {
    g_waiting_for_window_number = YES;
    if (is_modal)
      s_pending_display_is_modal = YES;
    return;
  }
  g_waiting_for_window_number = NO;
  if (s_pending_display_is_modal) {
    is_modal = YES;
    s_pending_display_is_modal = NO;
  }
  NotifyBrowserOfPluginShowWindow(window_info.window_id, window_info.bounds,
                                  is_modal);
}

}  // namespace

@interface NSWindow (ChromePluginUtilities)
// Returns YES if the window is visible and actually on the screen.
- (BOOL)chromePlugin_isWindowOnScreen;
@end

@implementation NSWindow (ChromePluginUtilities)

- (BOOL)chromePlugin_isWindowOnScreen {
  if (![self isVisible])
    return NO;
  NSRect window_frame = [self frame];
  for (NSScreen* screen in [NSScreen screens]) {
    if (NSIntersectsRect(window_frame, [screen frame]))
      return YES;
  }
  return NO;
}

@end

@interface NSWindow (ChromePluginInterposing)
- (void)chromePlugin_orderOut:(id)sender;
- (void)chromePlugin_orderFront:(id)sender;
- (void)chromePlugin_makeKeyAndOrderFront:(id)sender;
- (void)chromePlugin_setWindowNumber:(NSInteger)num;
@end

@implementation NSWindow (ChromePluginInterposing)

- (void)chromePlugin_orderOut:(id)sender {
  WindowInfo window_info(self);
  [self chromePlugin_orderOut:sender];
  OnPluginWindowClosed(window_info);
}

- (void)chromePlugin_orderFront:(id)sender {
  [self chromePlugin_orderFront:sender];
  if ([self chromePlugin_isWindowOnScreen])
    SwitchToPluginProcess();
  OnPluginWindowShown(WindowInfo(self), NO);
}

- (void)chromePlugin_makeKeyAndOrderFront:(id)sender {
  [self chromePlugin_makeKeyAndOrderFront:sender];
  if ([self chromePlugin_isWindowOnScreen])
    SwitchToPluginProcess();
  OnPluginWindowShown(WindowInfo(self), NO);
}

- (void)chromePlugin_setWindowNumber:(NSInteger)num {
  if (!g_waiting_for_window_number || num <= 0) {
    [self chromePlugin_setWindowNumber:num];
    return;
  }
  SwitchToPluginProcess();
  [self chromePlugin_setWindowNumber:num];
  OnPluginWindowShown(WindowInfo(self), NO);
}

@end

@interface NSApplication (ChromePluginInterposing)
- (NSInteger)chromePlugin_runModalForWindow:(NSWindow*)window;
@end

@implementation NSApplication (ChromePluginInterposing)

- (NSInteger)chromePlugin_runModalForWindow:(NSWindow*)window {
  SwitchToPluginProcess();
  // This is out-of-order relative to the other calls, but runModalForWindow:
  // won't return until the window closes, and the order only matters for
  // full-screen windows.
  OnPluginWindowShown(WindowInfo(window), YES);
  return [self chromePlugin_runModalForWindow:window];
}

@end

@interface NSCursor (ChromePluginInterposing)
- (void)chromePlugin_set;
+ (void)chromePlugin_hide;
+ (void)chromePlugin_unhide;
@end

@implementation NSCursor (ChromePluginInterposing)

- (void)chromePlugin_set {
  content::WebPluginDelegateImpl* delegate =
      content::WebPluginDelegateImpl::GetActiveDelegate();
  if (delegate) {
    delegate->SetNSCursor(self);
    return;
  }
  [self chromePlugin_set];
}

+ (void)chromePlugin_hide {
  NotifyPluginOfSetCursorVisibility(false);
}

+ (void)chromePlugin_unhide {
  NotifyPluginOfSetCursorVisibility(true);
}

@end

#pragma mark -

static void ExchangeMethods(Class target_class,
                            BOOL class_method,
                            SEL original,
                            SEL replacement) {
  Method m1;
  Method m2;
  if (class_method) {
    m1 = class_getClassMethod(target_class, original);
    m2 = class_getClassMethod(target_class, replacement);
  } else {
    m1 = class_getInstanceMethod(target_class, original);
    m2 = class_getInstanceMethod(target_class, replacement);
  }
  if (m1 && m2)
    method_exchangeImplementations(m1, m2);
  else
    NOTREACHED() << "Cocoa swizzling failed";
}

namespace mac_plugin_interposing {

void SetUpCocoaInterposing() {
  Class nswindow_class = [NSWindow class];
  ExchangeMethods(nswindow_class, NO, @selector(orderOut:),
                  @selector(chromePlugin_orderOut:));
  ExchangeMethods(nswindow_class, NO, @selector(orderFront:),
                  @selector(chromePlugin_orderFront:));
  ExchangeMethods(nswindow_class, NO, @selector(makeKeyAndOrderFront:),
                  @selector(chromePlugin_makeKeyAndOrderFront:));
  ExchangeMethods(nswindow_class, NO, @selector(_setWindowNumber:),
                  @selector(chromePlugin_setWindowNumber:));

  ExchangeMethods([NSApplication class], NO, @selector(runModalForWindow:),
                  @selector(chromePlugin_runModalForWindow:));

  Class nscursor_class = [NSCursor class];
  ExchangeMethods(nscursor_class, NO, @selector(set),
                  @selector(chromePlugin_set));
  ExchangeMethods(nscursor_class, YES, @selector(hide),
                  @selector(chromePlugin_hide));
  ExchangeMethods(nscursor_class, YES, @selector(unhide),
                  @selector(chromePlugin_unhide));
}

}  // namespace mac_plugin_interposing
