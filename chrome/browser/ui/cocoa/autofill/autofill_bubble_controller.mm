// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_bubble_controller.h"

#import "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "skia/ext/skia_utils_mac.h"

namespace {

// Border inset for error label.
const CGFloat kLabelInset = 3.0;

const CGFloat kMaxLabelWidth =
    2 * autofill::kFieldWidth + autofill::kHorizontalFieldPadding;

}  // namespace


@implementation AutofillBubbleController

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   message:(NSString*)message {
  return [self initWithParentWindow:parentWindow
                            message:message
                              inset:NSMakeSize(kLabelInset, kLabelInset)];
}

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   message:(NSString*)message
                     inset:(NSSize)inset {
  base::scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc] initWithContentRect:NSMakeRect(0, 0, 200, 100)
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO]);
  [window setAllowedAnimations:info_bubble::kAnimateNone];
  if ((self = [super initWithWindow:window
                       parentWindow:parentWindow
                         anchoredAt:NSZeroPoint])) {
    inset_ = inset;
    [self setShouldOpenAsKeyWindow:NO];
    [[self bubble] setArrowLocation:info_bubble::kTopCenter];
    [[self bubble] setAlignment:info_bubble::kAlignArrowToAnchor];

    label_.reset([[NSTextField alloc] init]);
    [label_ setEditable:NO];
    [label_ setBordered:NO];
    [label_ setDrawsBackground:NO];
    [label_ setStringValue:message];
    NSRect labelFrame = NSMakeRect(inset.width, inset.height, 0, 0);
    labelFrame.size = [[label_ cell] cellSizeForBounds:
        NSMakeRect(0, 0, kMaxLabelWidth, CGFLOAT_MAX)];
    [label_ setFrame:labelFrame];
    [[self bubble] addSubview:label_];

    NSRect windowFrame = [[self window] frame];
    windowFrame.size = NSMakeSize(
        NSMaxX([label_ frame]),
        NSHeight([label_ frame]) + info_bubble::kBubbleArrowHeight);
    windowFrame = NSInsetRect(windowFrame, -inset.width, -inset.height);
    [[self window] setFrame:windowFrame display:NO];
  }
  return self;
}

- (CGFloat)maxWidth {
  return kMaxLabelWidth + 2 * inset_.width;
}

@end
