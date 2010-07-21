// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/wrench_menu_controller.h"

#include "app/menus/menu_model.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#import "chrome/browser/cocoa/toolbar_controller.h"
#include "chrome/browser/wrench_menu_model.h"

@interface WrenchMenuController (Private)
- (WrenchMenuModel*)wrenchMenuModel;
- (void)adjustPositioning;
- (void)dispatchCommandInternal:(NSNumber*)tag;
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
      break;
    case IDC_ZOOM_MENU:
      DCHECK(zoomItem_);
      [customItem setView:zoomItem_];
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

  // NSSegmentedControls (used for the Edit item) need a little help to get the
  // command_id of the pressed item.
  if ([sender isKindOfClass:[NSSegmentedControl class]])
    tag = [[sender cell] tagForSegment:[sender selectedSegment]];

  // The custom views within the Wrench menu are abnormal and keep the menu open
  // after a target-action. Close the menu manually.
  // TODO(rsesek): It'd be great if the zoom buttons didn't have to close the
  // menu. See http://crbug.com/48679 for more info.
  [menu_ cancelTracking];
  // Executing certain commands from the nested run loop of the menu can lead
  // to wonky behavior (e.g. http://crbug.com/49716). To avoid this, schedule
  // the dispatch on the outermost run loop.
  [self performSelector:@selector(dispatchCommandInternal:)
             withObject:[NSNumber numberWithInt:tag]
             afterDelay:0.0];
}

// Used to perform the actual dispatch on the outermost runloop.
- (void)dispatchCommandInternal:(NSNumber*)tag {
  [self wrenchMenuModel]->ExecuteCommand([tag intValue]);
}

- (WrenchMenuModel*)wrenchMenuModel {
  return static_cast<WrenchMenuModel*>(model_);
}

// Fit the localized strings into the Cut/Copy/Paste control, then resize the
// whole menu item accordingly.
- (void)adjustPositioning {
  NSRect itemFrame = [editItem_ frame];
  NSRect controlFrame = [editControl_ frame];

  CGFloat originalControlWidth = NSWidth(controlFrame);
  // Maintain the carefully pixel-pushed gap between the edge of the menu and
  // the rightmost control.
  CGFloat edge = NSWidth(itemFrame) -
      (controlFrame.origin.x + originalControlWidth);

  // Resize the edit segmented control to fit the localized strings.
  [editControl_ sizeToFit];
  controlFrame = [editControl_ frame];
  CGFloat resizeAmount = NSWidth(controlFrame) - originalControlWidth;

  // Adjust the size of the entire menu item to account for changes in the size
  // of the segmented control.
  itemFrame.size.width += resizeAmount;
  [editItem_ setFrame:itemFrame];

  // Keep the spacing between the right edges of the menu and the control.
  controlFrame.origin.x = NSWidth(itemFrame) - edge - NSWidth(controlFrame);
  [editControl_ setFrame:controlFrame];
}

@end  // @implementation WrenchMenuController
