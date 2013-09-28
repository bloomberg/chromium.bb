// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_suggestion_container.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"
#include "skia/ext/skia_utils_mac.h"

namespace {

// Horizontal padding between text and other elements (in pixels).
const int kAroundTextPadding = 4;

// Vertical padding between individual elements.
const int kVerticalPadding = 8;

// Centers |rect2| vertically in |rect1|, on an integral y coordinate.
// Assumes |rect1| has integral origin coordinates.
NSRect CenterVertically(NSRect rect1, NSRect rect2) {
  DCHECK_LE(NSHeight(rect2), NSHeight(rect1));
  CGFloat offset = (NSHeight(rect1) - NSHeight(rect2)) / 2.0;
  rect2.origin.y = std::floor(rect1.origin.y + offset);
  return rect2;
}

}

@implementation AutofillSuggestionContainer

- (AutofillTextField*)inputField {
  return inputField_.get();
}

- (NSTextField*)makeDetailSectionLabel:(NSString*)labelText {
  base::scoped_nsobject<NSTextField> label([[NSTextField alloc] init]);
  [label setFont:
      [[NSFontManager sharedFontManager] convertFont:[label font]
                                         toHaveTrait:NSBoldFontMask]];
  [label setStringValue:labelText];
  [label setEditable:NO];
  [label setBordered:NO];
  [label sizeToFit];
  return label.autorelease();
}

- (void)loadView {
  label_.reset([[self createLabelWithFrame:NSZeroRect] retain]);
  label2_.reset([[self createLabelWithFrame:NSZeroRect] retain]);

  iconImageView_.reset([[NSImageView alloc] initWithFrame:NSZeroRect]);
  [iconImageView_ setImageFrameStyle:NSImageFrameNone];
  [iconImageView_ setHidden:YES];

  inputField_.reset([[AutofillTextField alloc] initWithFrame:NSZeroRect]);
  [inputField_ setHidden:YES];

  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);
  [view setSubviews:
      @[iconImageView_, label_, inputField_, label2_ ]];
  [self setView:view];
}

- (NSTextField*)createLabelWithFrame:(NSRect)frame {
  base::scoped_nsobject<NSTextField> label(
      [[NSTextField alloc] initWithFrame:frame]);
  [label setEditable:NO];
  [label setDrawsBackground:NO];
  [label setBordered:NO];
  return label.autorelease();
}

// TODO(groby): Can we make all the individual setters private and just
// update state as a whole?
- (void)setIcon:(NSImage*)iconImage {
  [iconImageView_ setImage:iconImage];
  [iconImageView_ setHidden:!iconImage];
  [iconImageView_ setFrameSize:[[iconImageView_ image] size]];
}

- (void)setSuggestionText:(NSString*)line1
                    line2:(NSString*)line2 {
  [label_ setStringValue:line1];
  [label2_ setStringValue:line2];
  [label2_ setHidden:![line2 length]];

  [label_ sizeToFit];
  [label2_ sizeToFit];
}

- (void)showInputField:(NSString*)text withIcon:(NSImage*)icon {
  [[inputField_ cell] setPlaceholderString:text];
  [[inputField_ cell] setIcon:icon];
  [inputField_ setHidden:NO];
  [inputField_ sizeToFit];
}

- (NSSize)preferredSizeForFirstLine {
  NSSize size = [label_ bounds].size;
  if (![iconImageView_ isHidden]) {
    size.height = std::max(size.height, NSHeight([iconImageView_ frame]));
    size.width += NSWidth([iconImageView_ frame]) + kAroundTextPadding;
  }
  // Final inputField_ sizing/spacing depends on a TODO(estade) in Views code.
  if (![inputField_ isHidden]) {
    size.height = std::max(size.height, NSHeight([inputField_ frame]));
    size.width += NSWidth([inputField_ frame])  + kAroundTextPadding;
  }
  return size;
}

- (NSSize)preferredSize {
  NSSize size = [self preferredSizeForFirstLine];

  if (![label2_ isHidden]) {
    NSSize size2 = [[label2_ cell] cellSize];
    size = NSMakeSize(
        std::ceil(std::max(size.width, size2.width)),
        std::ceil(size.height) + kVerticalPadding + std::ceil(size2.height));
  }
  return size;
}

- (void)performLayout {
  NSRect bounds = [[self view] bounds];
  NSSize preferredContainerSize = [self preferredSize];
  // width is externally determined.
  preferredContainerSize.width = NSWidth(bounds);

  NSRect lineFrame;
  lineFrame.size = [self preferredSizeForFirstLine];
  lineFrame.origin.x = NSMinX(bounds);
  lineFrame.origin.y = preferredContainerSize.height - NSHeight(lineFrame);

  // Ensure text does not exceed the view width.
  lineFrame.size.width = std::min(NSWidth(lineFrame), NSWidth(bounds));

  // Layout the controls - two lines on two rows, left-aligned. The optional
  // textfield is on the first line, right-aligned. The icon is also on the
  // first row, in front of the label.
  NSRect labelFrame = lineFrame;
  labelFrame.size.height = [[label_ cell] cellSize].height;
  labelFrame = CenterVertically(lineFrame, labelFrame);

  if (![iconImageView_ isHidden]) {
    NSRect dummyFrame;
    NSRect iconFrame = [iconImageView_ frame];
    iconFrame.origin = bounds.origin;
    iconFrame = CenterVertically(lineFrame, iconFrame);
    [iconImageView_ setFrameOrigin:iconFrame.origin];
    NSDivideRect(labelFrame, &dummyFrame, &labelFrame,
                 NSWidth(iconFrame) + kAroundTextPadding, NSMinXEdge);
  }

  if (![inputField_ isHidden]) {
    NSRect dummyFrame;
    NSRect inputfieldFrame = [inputField_ frame];
    inputfieldFrame.origin.x = NSMaxX(bounds) - NSWidth(inputfieldFrame);
    inputfieldFrame = CenterVertically(lineFrame, inputfieldFrame);
    [inputField_ setFrameOrigin:inputfieldFrame.origin];
    NSDivideRect(labelFrame, &dummyFrame, &labelFrame,
                 NSWidth(inputfieldFrame) + kAroundTextPadding, NSMaxXEdge);
  }

  [label_ setFrame:labelFrame];
  [label2_ setFrameOrigin:NSMakePoint(
          0,
          NSMinY(lineFrame) - kAroundTextPadding - NSHeight([label2_ frame]))];
  [[self view] setFrameSize:preferredContainerSize];
}

@end
