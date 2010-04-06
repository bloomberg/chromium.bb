// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/l10n_util.h"

#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

namespace cocoa_l10n_util {

NSInteger CompareFrameY(id view1, id view2, void* context) {
  CGFloat y1 = NSMinY([view1 frame]);
  CGFloat y2 = NSMinY([view2 frame]);
  if (y1 < y2)
    return NSOrderedAscending;
  else if (y1 > y2)
    return NSOrderedDescending;
  else
    return NSOrderedSame;
}

NSSize WrapOrSizeToFit(NSView* view) {
  if ([view isKindOfClass:[NSTextField class]]) {
    NSTextField* textField = static_cast<NSTextField*>(view);
    if ([textField isEditable])
      return NSZeroSize;
    CGFloat heightChange =
        [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:textField];
    return NSMakeSize(0.0, heightChange);
  }
  if ([view isKindOfClass:[NSMatrix class]]) {
    NSMatrix* radioGroup = static_cast<NSMatrix*>(view);
    [GTMUILocalizerAndLayoutTweaker wrapRadioGroupForWidth:radioGroup];
    return [GTMUILocalizerAndLayoutTweaker sizeToFitView:view];
  }
  if ([view isKindOfClass:[NSButton class]]) {
    NSButton* button = static_cast<NSButton*>(view);
    NSButtonCell* buttonCell = [button cell];
    // Decide it's a checkbox via showsStateBy and highlightsBy.
    if (([buttonCell showsStateBy] == NSCellState) &&
        ([buttonCell highlightsBy] == NSCellState)) {
      [GTMUILocalizerAndLayoutTweaker wrapButtonTitleForWidth:button];
      return [GTMUILocalizerAndLayoutTweaker sizeToFitView:view];
    }
  }
  return [GTMUILocalizerAndLayoutTweaker sizeToFitView:view];
}

}  // namespace cocoa_l10n_util
