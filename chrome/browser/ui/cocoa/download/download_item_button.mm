// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/download/download_item_button.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/download/download_item_cell.h"
#import "chrome/browser/ui/cocoa/download/download_item_controller.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_context_menu_controller.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#import "ui/base/cocoa/nsview_additions.h"

@implementation DownloadItemButton

@synthesize download = downloadPath_;
@synthesize controller = controller_;

// Overridden from DraggableButton.
- (void)beginDrag:(NSEvent*)event {
  if (!downloadPath_.empty()) {
    NSString* filename = base::SysUTF8ToNSString(downloadPath_.value());
    [self dragFile:filename fromRect:[self bounds] slideBack:YES event:event];
  }
}

// Override to show a context menu on mouse down if clicked over the context
// menu area.
- (void)mouseDown:(NSEvent*)event {
  DCHECK(controller_);
  // Override so that we can pop up a context menu on mouse down.
  NSCell* cell = [self cell];
  DCHECK([cell respondsToSelector:@selector(isMouseOverButtonPart)]);
  if ([reinterpret_cast<DownloadItemCell*>(cell) isMouseOverButtonPart]) {
    [self.draggableButton mouseDownImpl:event];
  } else {
    base::scoped_nsobject<DownloadShelfContextMenuController> menuController(
        [[DownloadShelfContextMenuController alloc]
            initWithItemController:controller_
                      withDelegate:self]);

    [cell setHighlighted:YES];
    [NSMenu popUpContextMenu:[menuController menu]
                   withEvent:[NSApp currentEvent]
                     forView:self];
  }
}

// Override to retain the controller, in case a closure is pumped that deletes
// the DownloadItemController while the menu is open <http://crbug.com/129826>.
- (void)rightMouseDown:(NSEvent*)event {
  base::scoped_nsobject<DownloadItemController> ref([controller_ retain]);
  [super rightMouseDown:event];
}

- (void)menuDidClose:(NSMenu*)menu {
  [[self cell] setHighlighted:NO];
}

- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent*)event {
  return YES;
}

- (BOOL)isOpaque {
  // Make this control opaque so that sub-pixel anti-aliasing works when
  // CoreAnimation is enabled.
  return YES;
}

- (void)drawRect:(NSRect)rect {
  NSView* downloadShelfView = [self ancestorWithViewID:VIEW_ID_DOWNLOAD_SHELF];
  [self cr_drawUsingAncestor:downloadShelfView inRect:rect];
  [super drawRect:rect];
}

@end
