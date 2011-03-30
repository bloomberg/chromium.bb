// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/download/download_item_button.h"

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/download/download_item_cell.h"
#import "chrome/browser/ui/cocoa/download/download_item_controller.h"

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
    [super mouseDown:event];
  } else {
    // Hold a reference to our controller in case the download completes and we
    // represent a file that's auto-removed (e.g. a theme).
    scoped_nsobject<DownloadItemController> ref([controller_ retain]);
    [cell setHighlighted:YES];
    [[self menu] setDelegate:self];
    [NSMenu popUpContextMenu:[self menu]
                   withEvent:[NSApp currentEvent]
                     forView:self];
  }
}

- (void)menuDidClose:(NSMenu*)menu {
  [[self cell] setHighlighted:NO];
}

- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent*)event {
  return YES;
}

@end
