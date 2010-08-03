// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/wrench_menu_controller.h"

#include "app/menus/menu_model.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#import "chrome/browser/cocoa/menu_tracked_root_view.h"
#import "chrome/browser/cocoa/toolbar_controller.h"
#include "chrome/browser/wrench_menu_model.h"

@interface WrenchMenuController (Private)
- (void)adjustPositioning;
- (void)performCommandDispatch:(NSNumber*)tag;
@end

@implementation WrenchMenuController

- (id)init {
  self = [super init];
  return self;
}

- (void)addItemToMenu:(NSMenu*)menu
              atIndex:(NSInteger)index
            fromModel:(menus::MenuModel*)model
           modelIndex:(int)modelIndex {
  // Non-button item types should be built as normal items.
  menus::MenuModel::ItemType type = model->GetTypeAt(modelIndex);
  if (type != menus::MenuModel::TYPE_BUTTON_ITEM) {
    [super addItemToMenu:menu
                 atIndex:index
               fromModel:model
              modelIndex:modelIndex];
    return;
  }

  // Handle the special-cased menu items.
  int command_id = model->GetCommandIdAt(modelIndex);
  scoped_nsobject<NSMenuItem> customItem(
      [[NSMenuItem alloc] initWithTitle:@""
                                 action:nil
                          keyEquivalent:@""]);
  switch (command_id) {
    case IDC_EDIT_MENU:
      DCHECK(editItem_);
      [customItem setView:editItem_];
      [editItem_ setMenuItem:customItem];
      break;
    case IDC_ZOOM_MENU:
      DCHECK(zoomItem_);
      [customItem setView:zoomItem_];
      [zoomItem_ setMenuItem:customItem];
      break;
    default:
      NOTREACHED();
      break;
  }
  [self adjustPositioning];
  [menu insertItem:customItem.get() atIndex:index];
}

- (NSMenu*)menu {
  NSMenu* menu = [super menu];
  if (![menu delegate]) {
    [menu setDelegate:self];
  }
  return menu;
}

- (void)menuWillOpen:(NSMenu*)menu {
  NSString* title = base::SysUTF16ToNSString(
      [self wrenchMenuModel]->GetLabelForCommandId(IDC_ZOOM_PERCENT_DISPLAY));
  [[zoomItem_ viewWithTag:IDC_ZOOM_PERCENT_DISPLAY] setTitle:title];

  bool plusEnabled = [self wrenchMenuModel]->IsCommandIdEnabled(IDC_ZOOM_PLUS);
  bool minusEnabled = [self wrenchMenuModel]->IsCommandIdEnabled(
      IDC_ZOOM_MINUS);
  [zoomPlus_ setEnabled:plusEnabled];
  [zoomMinus_ setEnabled:minusEnabled];

  NSImage* icon = [self wrenchMenuModel]->browser()->window()->IsFullscreen() ?
      [NSImage imageNamed:NSImageNameExitFullScreenTemplate] :
          [NSImage imageNamed:NSImageNameEnterFullScreenTemplate];
  [zoomFullScreen_ setImage:icon];
}

// Used to dispatch commands from the Wrench menu. The custom items within the
// menu cannot be hooked up directly to First Responder because the window in
// which the controls reside is not the BrowserWindowController, but a
// NSCarbonMenuWindow; this screws up the typical |-commandDispatch:| system.
- (IBAction)dispatchWrenchMenuCommand:(id)sender {
  NSInteger tag = [sender tag];
  // The custom views within the Wrench menu are abnormal and keep the menu open
  // after a target-action. Close the menu manually.
  // TODO(rsesek): It'd be great if the zoom buttons didn't have to close the
  // menu. See http://crbug.com/48679 for more info.
  [menu_ cancelTracking];
  [self dispatchCommandInternal:tag];
}

- (void)dispatchCommandInternal:(NSInteger)tag {
  // Executing certain commands from the nested run loop of the menu can lead
  // to wonky behavior (e.g. http://crbug.com/49716). To avoid this, schedule
  // the dispatch on the outermost run loop.
  [self performSelector:@selector(performCommandDispatch:)
             withObject:[NSNumber numberWithInt:tag]
             afterDelay:0.0];
}

// Used to perform the actual dispatch on the outermost runloop.
- (void)performCommandDispatch:(NSNumber*)tag {
  [self wrenchMenuModel]->ExecuteCommand([tag intValue]);
}

- (WrenchMenuModel*)wrenchMenuModel {
  return static_cast<WrenchMenuModel*>(model_);
}

// Fit the localized strings into the Cut/Copy/Paste control, then resize the
// whole menu item accordingly.
- (void)adjustPositioning {
  const CGFloat kButtonPadding = 12;
  CGFloat delta = 0;

  // Go through the three buttons from right-to-left, adjusting the size to fit
  // the localized strings while keeping them all aligned on their horizontal
  // edges.
  const size_t kAdjustViewCount = 3;
  NSButton* views[kAdjustViewCount] = { editPaste_, editCopy_, editCut_ };
  for (size_t i = 0; i < kAdjustViewCount; ++i) {
    NSButton* button = views[i];
    CGFloat originalWidth = NSWidth([button frame]);

    // Do not let |-sizeToFit| change the height of the button.
    NSSize size = [button frame].size;
    [button sizeToFit];
    size.width = [button frame].size.width + kButtonPadding;
    [button setFrameSize:size];

    CGFloat newWidth = size.width;
    delta += newWidth - originalWidth;

    NSRect frame = [button frame];
    frame.origin.x -= delta;
    [button setFrame:frame];
  }

  // Resize the menu item by the total amound the buttons changed so that the
  // spacing between the buttons and the title remains the same.
  NSRect itemFrame = [editItem_ frame];
  itemFrame.size.width += delta;
  [editItem_ setFrame:itemFrame];

  // Also resize the superview of the buttons, which is an NSView used to slide
  // when the item title is too big and GTM resizes it.
  NSRect parentFrame = [[editCut_ superview] frame];
  parentFrame.size.width += delta;
  parentFrame.origin.x -= delta;
  [[editCut_ superview] setFrame:parentFrame];
}

@end  // @implementation WrenchMenuController
