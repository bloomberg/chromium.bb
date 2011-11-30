// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu_mac.h"

#include "base/compiler_specific.h"
#import "base/mac/scoped_sending_event.h"
#include "base/memory/scoped_nsobject.h"
#include "base/message_loop.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#include "grit/generated_resources.h"

namespace {

// Retrieves an NSMenuItem which has the specified command_id. This function
// traverses the given |model| in the depth-first order. When this function
// finds an item whose command_id is the same as the given |command_id|, it
// returns the NSMenuItem associated with the item. This function emulates
// views::MenuItemViews::GetMenuItemByID() for Mac.
NSMenuItem* GetMenuItemByID(ui::MenuModel* model,
                            NSMenu* menu,
                            int command_id) {
  for (int i = 0; i < model->GetItemCount(); ++i) {
    NSMenuItem* item = [menu itemAtIndex:i];
    if (model->GetCommandIdAt(i) == command_id)
      return item;

    ui::MenuModel* submenu = model->GetSubmenuModelAt(i);
    if (submenu && [item hasSubmenu]) {
      NSMenuItem* subitem = GetMenuItemByID(submenu,
                                            [item submenu],
                                            command_id);
      if (subitem)
        return subitem;
    }
  }
  return nil;
}

}  // namespace

// Obj-C bridge class that is the target of all items in the context menu.
// Relies on the tag being set to the command id.

RenderViewContextMenuMac::RenderViewContextMenuMac(
    TabContents* web_contents,
    const ContextMenuParams& params,
    NSView* parent_view)
    : RenderViewContextMenu(web_contents, params),
      parent_view_(parent_view) {
}

RenderViewContextMenuMac::~RenderViewContextMenuMac() {
}

void RenderViewContextMenuMac::PlatformInit() {
  InitPlatformMenu();
  menuController_.reset(
      [[MenuController alloc] initWithModel:&menu_model_
                     useWithPopUpButtonCell:NO]);

  // Synthesize an event for the click, as there is no certainty that
  // [NSApp currentEvent] will return a valid event.
  NSEvent* currentEvent = [NSApp currentEvent];
  NSWindow* window = [parent_view_ window];
  NSPoint position = [window mouseLocationOutsideOfEventStream];
  NSTimeInterval eventTime = [currentEvent timestamp];
  NSEvent* clickEvent = [NSEvent mouseEventWithType:NSRightMouseDown
                                           location:position
                                      modifierFlags:NSRightMouseDownMask
                                          timestamp:eventTime
                                       windowNumber:[window windowNumber]
                                            context:nil
                                        eventNumber:0
                                         clickCount:1
                                           pressure:1.0];

  {
    // Make sure events can be pumped while the menu is up.
    MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());

    // One of the events that could be pumped is |window.close()|.
    // User-initiated event-tracking loops protect against this by
    // setting flags in -[CrApplication sendEvent:], but since
    // web-content menus are initiated by IPC message the setup has to
    // be done manually.
    base::mac::ScopedSendingEvent sendingEventScoper;

    // Show the menu.
    [NSMenu popUpContextMenu:[menuController_ menu]
                   withEvent:clickEvent
                     forView:parent_view_];
  }
}

void RenderViewContextMenuMac::ExecuteCommand(int id) {
  // Auxiliary windows that do not have address bars (Panels for example)
  // may not have Instant support.
  NSWindow* parent_window = [parent_view_ window];
  BrowserWindowController* controller =
      [BrowserWindowController browserWindowControllerForWindow:parent_window];
  [controller commitInstant];  // It's ok if controller is nil.

  RenderViewContextMenu::ExecuteCommand(id);
}

bool RenderViewContextMenuMac::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void RenderViewContextMenuMac::InitPlatformMenu() {
  bool has_selection = !params_.selection_text.empty();

  if (has_selection) {
      menu_model_.AddSeparator();
      menu_model_.AddItemWithStringId(
          IDC_CONTENT_CONTEXT_LOOK_UP_IN_DICTIONARY,
          IDS_CONTENT_CONTEXT_LOOK_UP_IN_DICTIONARY);
  }

}

void RenderViewContextMenuMac::LookUpInDictionary() {
  // TODO(morrita): On Safari, A dictionary panel could be shown
  // based on a preference setting of Dictionary.app.  We currently
  // don't support it: http://crbug.com/17951
  NSString* text = base::SysUTF16ToNSString(params_.selection_text);
  NSPasteboard* pboard = [NSPasteboard pasteboardWithUniqueName];
  // 10.5 and earlier require declareTypes before setData.
  // See the documentation on [NSPasteboard declareTypes].
  NSArray* toDeclare = [NSArray arrayWithObject:NSStringPboardType];
  [pboard declareTypes:toDeclare owner:nil];
  BOOL ok = [pboard setString:text forType:NSStringPboardType];
  if (ok)
    NSPerformService(@"Look Up in Dictionary", pboard);
}

void RenderViewContextMenuMac::UpdateMenuItem(int command_id,
                                              bool enabled,
                                              bool hidden,
                                              const string16& title) {
  NSMenuItem* item = GetMenuItemByID(&menu_model_, [menuController_ menu],
                                     command_id);
  if (!item)
    return;

  // Update the returned NSMenuItem directly so we can update it immediately.
  [item setEnabled:enabled];
  [item setTitle:SysUTF16ToNSString(title)];
  [item setHidden:hidden];
  [[item menu] itemChanged:item];
}
