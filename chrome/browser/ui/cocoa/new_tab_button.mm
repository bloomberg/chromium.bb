// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/new_tab_button.h"
#import "chrome/browser/ui/cocoa/image_button_cell.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

// A simple override of the ImageButtonCell to disable handling of
// -mouseEntered.
@interface NewTabButtonCell : ImageButtonCell

- (void)mouseEntered:(NSEvent*)theEvent;

@end

@implementation NewTabButtonCell

- (void)mouseEntered:(NSEvent*)theEvent {
  // Ignore this since the NTB enter is handled by the TabStripController.
}

@end


@implementation NewTabButton

+ (Class)cellClass {
  return [NewTabButtonCell class];
}

- (BOOL)pointIsOverButton:(NSPoint)point {
  NSPoint localPoint = [self convertPoint:point fromView:[self superview]];
  NSRect pointRect = NSMakeRect(localPoint.x, localPoint.y, 1, 1);
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  NSImage* buttonMask =
      bundle.GetNativeImageNamed(IDR_NEWTAB_BUTTON_MASK).ToNSImage();
  NSRect destinationRect = NSMakeRect(
      (NSWidth(self.bounds) - [buttonMask size].width) / 2,
      (NSHeight(self.bounds) - [buttonMask size].height) / 2,
      [buttonMask size].width, [buttonMask size].height);
  return [buttonMask hitTestRect:pointRect
        withImageDestinationRect:destinationRect
                         context:nil
                           hints:nil
                         flipped:YES];
}

// Override to only accept clicks within the bounds of the defined path, not
// the entire bounding box. |aPoint| is in the superview's coordinate system.
- (NSView*)hitTest:(NSPoint)aPoint {
  if ([self pointIsOverButton:aPoint])
    return [super hitTest:aPoint];
  return nil;
}

@end
