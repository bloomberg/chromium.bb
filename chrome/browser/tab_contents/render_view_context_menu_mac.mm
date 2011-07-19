// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu_mac.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_nsobject.h"
#include "base/message_loop.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#include "grit/generated_resources.h"

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

    // Show the menu.
    [NSMenu popUpContextMenu:[menuController_ menu]
                   withEvent:clickEvent
                     forView:parent_view_];
  }
}

void RenderViewContextMenuMac::ExecuteCommand(int id) {
  [[[parent_view_ window] windowController] commitInstant];
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
