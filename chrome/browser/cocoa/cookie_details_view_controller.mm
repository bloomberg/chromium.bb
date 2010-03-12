// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/cookie_details_view_controller.h"

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#import "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/cookie_tree_node.h"
#import "chrome/browser/cookie_modal_dialog.h"

namespace {
static const int kMinimalLabelOffsetFromViewBottom = 20;
}

#pragma mark View Controller

@implementation CookieDetailsViewController

- (id)init {
  return [super initWithNibName:@"CookieDetailsView"
                         bundle:mac_util::MainAppBundle()];
}

- (void)awakeFromNib {
  DCHECK(objectController_);
}

// Finds and returns the y offset of the lowest-most non-hidden
// text field in the view. This is used to shrink the view
// appropriately so that it just fits its visible content.
- (void)getLowestLabelVerticalPosition:(NSView*)view
                   lowestLabelPosition:(float&)lowestLabelPosition {
  if (![view isHidden]) {
    if ([view isKindOfClass:[NSTextField class]]) {
      NSRect frame = [view frame];
      if (frame.origin.y < lowestLabelPosition) {
        lowestLabelPosition = frame.origin.y;
      }
    }
    for (NSView* subview in [view subviews]) {
      [self getLowestLabelVerticalPosition:subview
                       lowestLabelPosition:lowestLabelPosition];
    }
  }
}

- (void)setContentObject:(id)content {
  [objectController_ setValue:content forKey:@"content"];
}

- (void)shrinkViewToFit {
  // Adjust the information pane to be exactly the right size
  // to hold the visible text information fields.
  NSView* view = [self view];
  NSRect frame = [view frame];
  float lowestLabelPosition = frame.origin.y + frame.size.height;
  [self getLowestLabelVerticalPosition:view
                   lowestLabelPosition:lowestLabelPosition];
  float verticalDelta = lowestLabelPosition - frame.origin.y -
      kMinimalLabelOffsetFromViewBottom;
  frame.origin.y += verticalDelta;
  frame.size.height -= verticalDelta;
  [[self view] setFrame:frame];
}

- (void)configureBindingsForTreeController:(NSTreeController*)treeController {
  // There seems to be a bug in the binding logic that it's not possible
  // to bind to the selection of the tree controller, the bind seems to
  // require an additional path segment in the key, thus the use of
  // selection.self rather than just selection below.
  [objectController_ bind:@"contentObject"
                 toObject:treeController
              withKeyPath:@"selection.self"
                  options:nil];
}

@end
