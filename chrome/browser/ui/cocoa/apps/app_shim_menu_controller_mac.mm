// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/apps/app_shim_menu_controller_mac.h"

#include "apps/app_shim/extension_app_shim_handler_mac.h"
#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/apps/native_app_window_cocoa.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface AppShimMenuController ()
// Construct the NSMenuItems for apps.
- (void)buildAppMenuItems;
// Register for NSWindow notifications.
- (void)registerEventHandlers;
// If the window is an app window, add or remove menu items.
- (void)windowMainStatusChanged:(NSNotification*)notification;
// Add menu items for an app and hide Chrome menu items.
- (void)addMenuItems:(const extensions::Extension*)app;
// If the window belongs to the currently focused app, remove the menu items and
// unhide Chrome menu items.
- (void)removeMenuItems:(NSString*)appId;
// If the currently focused window belongs to a platform app, quit the app.
- (void)quitCurrentPlatformApp;
@end

@implementation AppShimMenuController

- (id)init {
  if ((self = [super init])) {
    [self buildAppMenuItems];
    [self registerEventHandlers];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)buildAppMenuItems {
  // Find the "Quit Chrome" menu item.
  NSMenu* chromeMenu = [[[NSApp mainMenu] itemAtIndex:0] submenu];
  for (NSMenuItem* item in [chromeMenu itemArray]) {
    if ([item action] == @selector(terminate:)) {
      chromeMenuQuitItem_.reset([item retain]);
      break;
    }
  }
  DCHECK(chromeMenuQuitItem_);

  appMenuItem_.reset([[NSMenuItem alloc] initWithTitle:@""
                                                action:nil
                                         keyEquivalent:@""]);
  base::scoped_nsobject<NSMenu> appMenu([[NSMenu alloc] initWithTitle:@""]);
  [appMenu setAutoenablesItems:NO];
  NSMenuItem* appMenuQuitItem =
      [appMenu addItemWithTitle:@""
                         action:@selector(quitCurrentPlatformApp)
                  keyEquivalent:@"q"];
  [appMenuQuitItem setKeyEquivalentModifierMask:
      [chromeMenuQuitItem_ keyEquivalentModifierMask]];
  [appMenuQuitItem setTarget:self];
  [appMenuItem_ setSubmenu:appMenu];
}

- (void)registerEventHandlers {
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(windowMainStatusChanged:)
             name:NSWindowDidBecomeMainNotification
           object:nil];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(windowMainStatusChanged:)
             name:NSWindowDidResignMainNotification
           object:nil];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(windowMainStatusChanged:)
             name:NSWindowWillCloseNotification
           object:nil];
}

- (void)windowMainStatusChanged:(NSNotification*)notification {
  id window = [notification object];
  id windowController = [window windowController];
  if (![windowController isKindOfClass:[NativeAppWindowController class]])
    return;

  apps::ShellWindow* shellWindow =
      apps::ShellWindowRegistry::GetShellWindowForNativeWindowAnyProfile(
          window);
  if (!shellWindow)
    return;

  NSString* name = [notification name];
  if ([name isEqualToString:NSWindowDidBecomeMainNotification]) {
    [self addMenuItems:shellWindow->extension()];
  } else if ([name isEqualToString:NSWindowDidResignMainNotification] ||
             [name isEqualToString:NSWindowWillCloseNotification]) {
    [self removeMenuItems:base::SysUTF8ToNSString(shellWindow->extension_id())];
  } else {
    NOTREACHED();
  }
}

- (void)addMenuItems:(const extensions::Extension*)app {
  NSString* appId = base::SysUTF8ToNSString(app->id());
  NSString* title = base::SysUTF8ToNSString(app->name());

  if ([appId_ isEqualToString:appId])
    return;

  [self removeMenuItems:appId_];
  appId_.reset([appId copy]);

  // Hide Chrome menu items.
  NSMenu* mainMenu = [NSApp mainMenu];
  for (NSMenuItem* item in [mainMenu itemArray])
    [item setHidden:YES];

  NSString* localizedQuitApp =
      l10n_util::GetNSStringF(IDS_EXIT_MAC, base::UTF8ToUTF16(app->name()));
  NSMenuItem* appMenuQuitItem = [[[appMenuItem_ submenu] itemArray] lastObject];
  [appMenuQuitItem setTitle:localizedQuitApp];

  // It seems that two menu items that have the same key equivalent must also
  // have the same action for the keyboard shortcut to work. (This refers to the
  // original keyboard shortcut, regardless of any overrides set in OSX).
  // In order to let the appMenuQuitItem have a different action, we remove the
  // key equivalent from the chromeMenuQuitItem and restore it later.
  [chromeMenuQuitItem_ setKeyEquivalent:@""];

  [appMenuItem_ setTitle:appId];
  [[appMenuItem_ submenu] setTitle:title];
  [mainMenu addItem:appMenuItem_];
}

- (void)removeMenuItems:(NSString*)appId {
  if (![appId_ isEqualToString:appId])
    return;

  appId_.reset();

  NSMenu* mainMenu = [NSApp mainMenu];
  [mainMenu removeItem:appMenuItem_];

  // Restore the Chrome main menu bar.
  for (NSMenuItem* item in [mainMenu itemArray])
    [item setHidden:NO];

  // Restore the keyboard shortcut to Chrome. This just needs to be set back to
  // the original keyboard shortcut, regardless of any overrides in OSX. The
  // overrides still work as they are based on the title of the menu item.
  [chromeMenuQuitItem_ setKeyEquivalent:@"q"];
}

- (void)quitCurrentPlatformApp {
  apps::ShellWindow* shellWindow =
      apps::ShellWindowRegistry::GetShellWindowForNativeWindowAnyProfile(
          [NSApp keyWindow]);
  if (shellWindow)
    apps::ExtensionAppShimHandler::QuitAppForWindow(shellWindow);
}

@end
