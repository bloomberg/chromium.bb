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
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"
#include "skia/ext/skia_utils_mac.h"

namespace {

// Horizontal padding between text and other elements (in pixels).
const int kAroundTextPadding = 4;

// Vertical padding between individual elements.
const int kVerticalPadding = 8;

// Padding at the top of suggestions.
const CGFloat kTopPadding = 10;

// Indicates infinite size in either vertical or horizontal direction.
// Technically, CGFLOAT_MAX should do. Practically, it runs into several issues.
// #1) Many computations on Retina devices overflow with that value.
// #2) In this particular use case, it results in the message
//     "CGAffineTransformInvert: singular matrix."
const CGFloat kInfiniteSize = 1.0e6;

// A line fragment padding that creates the same visual look as text layout in
// an NSTextField does. (Which UX feedback was based on)
const CGFloat kLineFragmentPadding = 2.0;

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
  label_.reset([[NSTextView alloc] initWithFrame:NSZeroRect]);
  [[label_ textContainer] setLineFragmentPadding:kLineFragmentPadding];
  [label_ setEditable:NO];
  [label_ setSelectable:NO];
  [label_ setDrawsBackground:NO];

  label2_.reset([[self createLabelWithFrame:NSZeroRect] retain]);

  iconImageView_.reset([[NSImageView alloc] initWithFrame:NSZeroRect]);
  [iconImageView_ setImageFrameStyle:NSImageFrameNone];
  [iconImageView_ setHidden:YES];

  inputField_.reset([[AutofillTextField alloc] initWithFrame:NSZeroRect]);
  [inputField_ setHidden:YES];

  spacer_.reset([[NSBox alloc] initWithFrame:NSZeroRect]);
  [spacer_ setBoxType:NSBoxSeparator];
  [spacer_ setBorderType:NSLineBorder];

  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);
  [view setSubviews:
      @[ iconImageView_, label_, inputField_, label2_, spacer_ ]];
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
  NSDictionary* attributes = @{
    NSCursorAttributeName : [NSCursor arrowCursor],
      NSFontAttributeName : [NSFont controlContentFontOfSize:0]
  };
  base::scoped_nsobject<NSAttributedString> str1(
      [[NSAttributedString alloc] initWithString:line1
                                      attributes:attributes]);
  [[label_ textStorage] setAttributedString:str1];

  [label2_ setStringValue:line2];
  [label2_ setHidden:![line2 length]];

  [label_ setVerticallyResizable:YES];
  [label_ setHorizontallyResizable:NO];
  [label_ setFrameSize:NSMakeSize(2 * autofill::kFieldWidth, kInfiniteSize)];
  [label_ sizeToFit];
  [label2_ sizeToFit];
}

- (void)showInputField:(NSString*)text withIcon:(NSImage*)icon {
  [[inputField_ cell] setPlaceholderString:text];
  [[inputField_ cell] setIcon:icon];
  [inputField_ setHidden:NO];
  [inputField_ sizeToFit];

  // Enforce fixed width.
  NSSize frameSize = NSMakeSize(autofill::kFieldWidth,
                                NSHeight([inputField_ frame]));
  [inputField_ setFrameSize:frameSize];
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

  size.height += kTopPadding;

  return size;
}

- (void)performLayout {
  NSRect bounds = [[self view] bounds];
  NSSize preferredContainerSize = [self preferredSize];
  // width is externally determined.
  preferredContainerSize.width = NSWidth(bounds);

  NSRect spacerFrame = NSMakeRect(0, preferredContainerSize.height - 1,
                                  preferredContainerSize.width, 1);

  NSRect lineFrame;
  lineFrame.size = [self preferredSizeForFirstLine];
  lineFrame.origin.x = NSMinX(bounds);
  lineFrame.origin.y =
      preferredContainerSize.height - NSHeight(lineFrame) - kTopPadding;

  // Ensure text does not exceed the view width.
  lineFrame.size.width = std::min(NSWidth(lineFrame), NSWidth(bounds));

  // Layout the controls - two lines on two rows, left-aligned. The optional
  // textfield is on the first line, right-aligned. The icon is also on the
  // first row, in front of the label.
  NSRect labelFrame = lineFrame;
  NSLayoutManager* layoutManager = [label_ layoutManager];
  NSTextContainer* textContainer = [label_ textContainer];
  [layoutManager ensureLayoutForTextContainer:textContainer];
  NSRect textFrame = [layoutManager usedRectForTextContainer:textContainer];

  labelFrame.size.height = textFrame.size.height;
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

  [spacer_ setFrame:spacerFrame];
  [label_ setFrame:labelFrame];
  [label2_ setFrameOrigin:NSMakePoint(
          0,
          NSMinY(lineFrame) - kAroundTextPadding - NSHeight([label2_ frame]))];
  [[self view] setFrameSize:preferredContainerSize];
}

@end
