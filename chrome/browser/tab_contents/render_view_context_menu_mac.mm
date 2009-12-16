// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu_mac.h"

#include "app/l10n_util_mac.h"
#include "base/compiler_specific.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"
#include "base/scoped_nsobject.h"

// Obj-C bridge class that is the target of all items in the context menu.
// Relies on the tag being set to the command id. Uses |context_| to
// execute the command once it is selected and determine if commands should
// be enabled or disabled.

@interface ContextMenuTarget : NSObject {
 @private
  RenderViewContextMenuMac* context_;  // weak, owns us.
}
- (id)initWithContext:(RenderViewContextMenuMac*)context;
- (void)itemSelected:(id)sender;
@end

@implementation ContextMenuTarget

- (id)initWithContext:(RenderViewContextMenuMac*)context {
  if ((self = [super init])) {
    DCHECK(context);
    context_ = context;
  }
  return self;
}

- (void)itemSelected:(id)sender {
  context_->ExecuteCommand([sender tag]);
}

@end

RenderViewContextMenuMac::RenderViewContextMenuMac(
    TabContents* web_contents,
    const ContextMenuParams& params,
    NSView* parent_view)
    : RenderViewContextMenu(web_contents, params),
      menu_([[NSMenu alloc] init]),
      insert_menu_(menu_),
      target_(nil),
      parent_view_(parent_view) {
  [menu_ setAutoenablesItems:NO];
  target_ = [[ContextMenuTarget alloc] initWithContext:this];
}

RenderViewContextMenuMac::~RenderViewContextMenuMac() {
  [target_ release];
  [menu_ release];
}

void RenderViewContextMenuMac::DoInit() {
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
    [NSMenu popUpContextMenu:menu_
                   withEvent:clickEvent
                     forView:parent_view_];
  }
}

// Do things like remove the windows accelerators.
// TODO(pinkerton): Do we want to do anything like make a maximum string width
// and middle-truncate?
NSString* RenderViewContextMenuMac::PrepareLabelForDisplay(
    const string16& label) {
  NSString* title = l10n_util::FixUpWindowsStyleLabel(label);
  DCHECK(title);
  return title ? title : @"";
}

void RenderViewContextMenuMac::AppendMenuItem(int command_id) {
  AppendMenuItem(command_id, l10n_util::GetStringUTF16(command_id));
}

void RenderViewContextMenuMac::AppendMenuItem(int command_id,
                                              const string16& label) {
  AppendMenuItemWithState(command_id, label, NSOffState);
}

void RenderViewContextMenuMac::AppendRadioMenuItem(int command_id,
                                                   const string16& label) {
  AppendMenuItemWithState(command_id, label,
                          ItemIsChecked(command_id) ? NSOnState : NSOffState);

}
void RenderViewContextMenuMac::AppendCheckboxMenuItem(int command_id,
    const string16& label) {
  AppendMenuItemWithState(command_id, label,
                          ItemIsChecked(command_id) ? NSOnState : NSOffState);
}

void RenderViewContextMenuMac::AppendSeparator() {
  NSMenuItem* separator = [NSMenuItem separatorItem];
  [insert_menu_ addItem:separator];
}

void RenderViewContextMenuMac::StartSubMenu(int command_id,
                                            const string16& label) {
  // I'm not a fan of this kind of API, but the other platforms have similar
  // guards so at least we know everyone will break together if someone
  // tries to mis-use the API.
  if (insert_menu_ != menu_) {
    NOTREACHED();
    return;
  }

  // We don't need to retain the submenu as the context menu already does, but
  // we switch the "insert menu" so subsequent items are added to the submenu
  // and not the main menu. This happens until someone calls FinishSubMenu().
  scoped_nsobject<NSMenuItem> submenu_item([[NSMenuItem alloc]
                              initWithTitle:PrepareLabelForDisplay(label)
                                     action:nil
                              keyEquivalent:@""]);
  insert_menu_ = [[[NSMenu alloc] init] autorelease];
  [submenu_item setSubmenu:insert_menu_];
  [menu_ addItem:submenu_item];
}

void RenderViewContextMenuMac::FinishSubMenu() {
  // Set the "insert menu" back to the main menu so that subsequently added
  // items get added to the main context menu.
  DCHECK(insert_menu_ != menu_);
  insert_menu_ = menu_;
}

void RenderViewContextMenuMac::AppendMenuItemWithState(int command_id,
                                                       const string16& label,
                                                       NSCellStateValue state) {
  scoped_nsobject<NSMenuItem> item([[NSMenuItem alloc]
                                    initWithTitle:PrepareLabelForDisplay(label)
                                           action:@selector(itemSelected:)
                                    keyEquivalent:@""]);
  [item setState:state];
  [item setTag:command_id];
  [item setTarget:target_];
  [item setEnabled:IsItemCommandEnabled(command_id) ? YES : NO];
  [insert_menu_ addItem:item];
}

