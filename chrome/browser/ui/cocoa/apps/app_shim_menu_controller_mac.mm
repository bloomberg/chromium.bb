// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/apps/app_shim_menu_controller_mac.h"

#include "apps/app_shim/extension_app_shim_handler_mac.h"
#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/apps/native_app_window_cocoa.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// Gets an item from the main menu given the tag of the top level item
// |menu_tag| and the tag of the item |item_tag|.
NSMenuItem* GetItemByTag(NSInteger menu_tag, NSInteger item_tag) {
  return [[[[NSApp mainMenu] itemWithTag:menu_tag] submenu]
      itemWithTag:item_tag];
}

// Finds a top level menu item using |menu_tag| and creates a new NSMenuItem
// with the same title.
NSMenuItem* NewTopLevelItemFrom(NSInteger menu_tag) {
  NSMenuItem* original = [[NSApp mainMenu] itemWithTag:menu_tag];
  base::scoped_nsobject<NSMenuItem> item([[NSMenuItem alloc]
      initWithTitle:[original title]
             action:nil
      keyEquivalent:@""]);
  DCHECK([original hasSubmenu]);
  base::scoped_nsobject<NSMenu> sub_menu([[NSMenu alloc]
      initWithTitle:[[original submenu] title]]);
  [item setSubmenu:sub_menu];
  return item.autorelease();
}

// Finds an item using |menu_tag| and |item_tag| and adds a duplicate of it to
// the submenu of |top_level_item|.
void AddDuplicateItem(NSMenuItem* top_level_item,
                      NSInteger menu_tag,
                      NSInteger item_tag) {
  base::scoped_nsobject<NSMenuItem> item(
      [GetItemByTag(menu_tag, item_tag) copy]);
  DCHECK(item);
  [[top_level_item submenu] addItem:item];
}

}  // namespace

// Used by AppShimMenuController to manage menu items that are a copy of a
// Chrome menu item but with a different action. This manages unsetting and
// restoring the original item's key equivalent, so that we can use the same
// key equivalent in the copied item with a different action.
@interface DoppelgangerMenuItem : NSObject {
 @private
  base::scoped_nsobject<NSMenuItem> menuItem_;
  base::scoped_nsobject<NSMenuItem> sourceItem_;
  base::scoped_nsobject<NSString> sourceKeyEquivalent_;
  int resourceId_;
}

@property(readonly, nonatomic) NSMenuItem* menuItem;

// Get the source item using the tags and create the menu item.
- (id)initWithController:(AppShimMenuController*)controller
                 menuTag:(NSInteger)menuTag
                 itemTag:(NSInteger)itemTag
              resourceId:(int)resourceId
                  action:(SEL)action
           keyEquivalent:(NSString*)keyEquivalent;
// Set the title using |resourceId_| and unset the source item's key equivalent.
- (void)enableForApp:(const extensions::Extension*)app;
// Restore the source item's key equivalent.
- (void)disable;
@end

@implementation DoppelgangerMenuItem

- (NSMenuItem*)menuItem {
  return menuItem_;
}

- (id)initWithController:(AppShimMenuController*)controller
                 menuTag:(NSInteger)menuTag
                 itemTag:(NSInteger)itemTag
              resourceId:(int)resourceId
                  action:(SEL)action
           keyEquivalent:(NSString*)keyEquivalent {
  if ((self = [super init])) {
    sourceItem_.reset([GetItemByTag(menuTag, itemTag) retain]);
    DCHECK(sourceItem_);
    sourceKeyEquivalent_.reset([[sourceItem_ keyEquivalent] copy]);
    menuItem_.reset([[NSMenuItem alloc]
        initWithTitle:@""
               action:action
        keyEquivalent:keyEquivalent]);
    [menuItem_ setTarget:controller];
    [menuItem_ setTag:itemTag];
    resourceId_ = resourceId;
  }
  return self;
}

- (void)enableForApp:(const extensions::Extension*)app {
  // It seems that two menu items that have the same key equivalent must also
  // have the same action for the keyboard shortcut to work. (This refers to the
  // original keyboard shortcut, regardless of any overrides set in OSX).
  // In order to let the app menu items have a different action, we remove the
  // key equivalent of the original items and restore them later.
  [sourceItem_ setKeyEquivalent:@""];
  if (!resourceId_)
    return;

  [menuItem_ setTitle:l10n_util::GetNSStringF(resourceId_,
                                              base::UTF8ToUTF16(app->name()))];
}

- (void)disable {
  // Restore the keyboard shortcut to Chrome. This just needs to be set back to
  // the original keyboard shortcut, regardless of any overrides in OSX. The
  // overrides still work as they are based on the title of the menu item.
  [sourceItem_ setKeyEquivalent:sourceKeyEquivalent_];
}

@end

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
// If the currently focused window belongs to a platform app, hide the app.
- (void)hideCurrentPlatformApp;
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
  hideDoppelganger_.reset([[DoppelgangerMenuItem alloc]
      initWithController:self
                 menuTag:IDC_CHROME_MENU
                 itemTag:IDC_HIDE_APP
              resourceId:IDS_HIDE_APP_MAC
                  action:@selector(hideCurrentPlatformApp)
           keyEquivalent:@"h"]);
  quitDoppelganger_.reset([[DoppelgangerMenuItem alloc]
      initWithController:self
                 menuTag:IDC_CHROME_MENU
                 itemTag:IDC_EXIT
              resourceId:IDS_EXIT_MAC
                  action:@selector(quitCurrentPlatformApp)
           keyEquivalent:@"q"]);

  // The app's menu.
  appMenuItem_.reset([[NSMenuItem alloc] initWithTitle:@""
                                                action:nil
                                         keyEquivalent:@""]);
  base::scoped_nsobject<NSMenu> appMenu([[NSMenu alloc] initWithTitle:@""]);
  [appMenuItem_ setSubmenu:appMenu];
  [appMenu setAutoenablesItems:NO];

  [appMenu addItem:[hideDoppelganger_ menuItem]];
  [appMenu addItem:[NSMenuItem separatorItem]];
  [appMenu addItem:[quitDoppelganger_ menuItem]];

  // File menu.
  fileMenuItem_.reset([NewTopLevelItemFrom(IDC_FILE_MENU) retain]);
  AddDuplicateItem(fileMenuItem_, IDC_FILE_MENU, IDC_CLOSE_WINDOW);

  // Edit menu. This is copied entirely.
  editMenuItem_.reset([[[NSApp mainMenu] itemWithTag:IDC_EDIT_MENU] copy]);

  // Window menu.
  windowMenuItem_.reset([NewTopLevelItemFrom(IDC_WINDOW_MENU) retain]);
  AddDuplicateItem(windowMenuItem_, IDC_WINDOW_MENU, IDC_MINIMIZE_WINDOW);
  AddDuplicateItem(windowMenuItem_, IDC_WINDOW_MENU, IDC_MAXIMIZE_WINDOW);
  [[windowMenuItem_ submenu] addItem:[NSMenuItem separatorItem]];
  AddDuplicateItem(windowMenuItem_, IDC_WINDOW_MENU, IDC_ALL_WINDOWS_FRONT);
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

  [hideDoppelganger_ enableForApp:app];
  [quitDoppelganger_ enableForApp:app];

  [appMenuItem_ setTitle:appId];
  [[appMenuItem_ submenu] setTitle:title];

  [mainMenu addItem:appMenuItem_];
  [mainMenu addItem:fileMenuItem_];
  [mainMenu addItem:editMenuItem_];
  [mainMenu addItem:windowMenuItem_];
}

- (void)removeMenuItems:(NSString*)appId {
  if (![appId_ isEqualToString:appId])
    return;

  appId_.reset();

  NSMenu* mainMenu = [NSApp mainMenu];
  [mainMenu removeItem:appMenuItem_];
  [mainMenu removeItem:fileMenuItem_];
  [mainMenu removeItem:editMenuItem_];
  [mainMenu removeItem:windowMenuItem_];

  // Restore the Chrome main menu bar.
  for (NSMenuItem* item in [mainMenu itemArray])
    [item setHidden:NO];

  [hideDoppelganger_ disable];
  [quitDoppelganger_ disable];
}

- (void)quitCurrentPlatformApp {
  apps::ShellWindow* shellWindow =
      apps::ShellWindowRegistry::GetShellWindowForNativeWindowAnyProfile(
          [NSApp keyWindow]);
  if (shellWindow)
    apps::ExtensionAppShimHandler::QuitAppForWindow(shellWindow);
}

- (void)hideCurrentPlatformApp {
  apps::ShellWindow* shellWindow =
      apps::ShellWindowRegistry::GetShellWindowForNativeWindowAnyProfile(
          [NSApp keyWindow]);
  if (shellWindow)
    apps::ExtensionAppShimHandler::HideAppForWindow(shellWindow);
}

@end
