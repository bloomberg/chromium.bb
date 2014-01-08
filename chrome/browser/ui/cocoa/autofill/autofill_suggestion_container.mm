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

// Padding added on top of the label so its first line looks centered with
// respect to the input field. Only added when the input field is showing.
const CGFloat kLabelWithInputTopPadding = 5.0;

}

// An attachment cell for a single icon - takes care of proper alignment of
// text and icon.
@interface IconAttachmentCell : NSTextAttachmentCell {
  CGFloat baseline_;  // The cell's baseline adjustment.
}

// Adjust the cell's baseline so that the lower edge of the image aligns with
// the longest descender, not the font baseline
- (void)adjustBaselineForFont:(NSFont*)font;

@end


@interface AutofillSuggestionView : NSView {
 @private
  // The main input field - only view not ignoring mouse events.
  NSView* inputField_;
}

@property (assign, nonatomic) NSView* inputField;

@end


// The suggestion container should ignore any mouse events unless they occur
// within the bounds of an editable field.
@implementation AutofillSuggestionView

@synthesize inputField = inputField_;

- (NSView*)hitTest:(NSPoint)point {
  NSView* hitView = [super hitTest:point];
  if ([hitView isDescendantOf:inputField_])
    return hitView;

  return nil;
}

@end


@implementation IconAttachmentCell

- (NSPoint)cellBaselineOffset {
  return NSMakePoint(0.0, baseline_);
}

// Ensure proper padding between text and icon.
- (NSSize)cellSize {
  NSSize size = [super cellSize];
  size.width += kAroundTextPadding;
  return size;
}

// drawWithFrame: needs to be overridden to left-align the image. Default
// rendering centers images in the cell's frame.
- (void)drawWithFrame:(NSRect)frame inView:(NSView*)view {
  frame.size.width -= kAroundTextPadding;
  [super drawWithFrame:frame inView:view];
}

- (void)adjustBaselineForFont:(NSFont*)font {
  CGFloat lineHeight = [font ascender];
  baseline_ = std::floor((lineHeight - [[self image] size].height) / 2.0);
}

@end


@interface AutofillSuggestionContainer (Private)

// Set the main suggestion text and the corresponding |icon|.
// Attempts to wrap the text if |wrapText| is set.
- (void)setSuggestionText:(NSString*)line
                     icon:(NSImage*)icon
                 wrapText:(BOOL)wrapText;

@end


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

  base::scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setLineSpacing:0.5 * [[label_ font] pointSize]];
  [label_ setDefaultParagraphStyle:paragraphStyle];

  inputField_.reset([[AutofillTextField alloc] initWithFrame:NSZeroRect]);
  [inputField_ setHidden:YES];

  spacer_.reset([[NSBox alloc] initWithFrame:NSZeroRect]);
  [spacer_ setBoxType:NSBoxSeparator];
  [spacer_ setBorderType:NSLineBorder];

  base::scoped_nsobject<AutofillSuggestionView> view(
      [[AutofillSuggestionView alloc] initWithFrame:NSZeroRect]);
  [view setSubviews:
      @[ label_, inputField_, spacer_ ]];
  [view setInputField:inputField_];
  [self setView:view];
}

- (void)setSuggestionText:(NSString*)line
                     icon:(NSImage*)icon
                 wrapText:(BOOL)wrapText {
  [label_ setString:@""];

  if ([icon size].width) {
    base::scoped_nsobject<IconAttachmentCell> cell(
        [[IconAttachmentCell alloc] initImageCell:icon]);
    base::scoped_nsobject<NSTextAttachment> attachment(
        [[NSTextAttachment alloc] init]);
    [cell adjustBaselineForFont:[NSFont controlContentFontOfSize:0]];
    [cell setAlignment:NSLeftTextAlignment];
    [attachment setAttachmentCell:cell];
    [[label_ textStorage] setAttributedString:
        [NSAttributedString attributedStringWithAttachment:attachment]];
  }

  NSDictionary* attributes = @{
    NSParagraphStyleAttributeName : [label_ defaultParagraphStyle],
            NSCursorAttributeName : [NSCursor arrowCursor],
              NSFontAttributeName : [NSFont controlContentFontOfSize:0]
  };
  base::scoped_nsobject<NSAttributedString> str1(
      [[NSAttributedString alloc] initWithString:line
                                      attributes:attributes]);
  [[label_ textStorage] appendAttributedString:str1];

  [label_ setVerticallyResizable:YES];
  [label_ setHorizontallyResizable:!wrapText];
  if (wrapText) {
    CGFloat availableWidth =
        4 * autofill::kFieldWidth - [inputField_ frame].size.width;
    [label_ setFrameSize:NSMakeSize(availableWidth, kInfiniteSize)];
  } else {
    [label_ setFrameSize:NSMakeSize(kInfiniteSize, kInfiniteSize)];
  }
  [[label_ layoutManager] ensureLayoutForTextContainer:[label_ textContainer]];
  [label_ sizeToFit];
}

- (void)
    setSuggestionWithVerticallyCompactText:(NSString*)verticallyCompactText
                   horizontallyCompactText:(NSString*)horizontallyCompactText
                                      icon:(NSImage*)icon
                                  maxWidth:(CGFloat)maxWidth {
  // Prefer the vertically compact text when it fits. If it doesn't fit, fall
  // back to the horizontally compact text.
  [self setSuggestionText:verticallyCompactText icon:icon wrapText:NO];
  if ([self preferredSize].width > maxWidth)
    [self setSuggestionText:horizontallyCompactText icon:icon wrapText:YES];
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


- (NSSize)preferredSize {
  NSSize size = [label_ bounds].size;

  // Final inputField_ sizing/spacing depends on a TODO(estade) in Views code.
  if (![inputField_ isHidden]) {
    size.height = std::max(size.height + kLabelWithInputTopPadding,
                           NSHeight([inputField_ frame]));
    size.width += NSWidth([inputField_ frame])  + kAroundTextPadding;
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

  NSRect labelFrame = [label_ bounds];
  labelFrame.origin.x = NSMinX(bounds);
  labelFrame.origin.y = NSMaxY(bounds) - NSHeight(labelFrame) - kTopPadding;

  // Position input field - top is aligned to top of label field.
  if (![inputField_ isHidden]) {
    NSRect inputFieldFrame = [inputField_ frame];
    inputFieldFrame.origin.x = NSMaxX(bounds) - NSWidth(inputFieldFrame);
    inputFieldFrame.origin.y = NSMaxY(labelFrame) - NSHeight(inputFieldFrame);
    [inputField_ setFrameOrigin:inputFieldFrame.origin];

    // Vertically center the first line of the label with respect to the input
    // field.
    labelFrame.origin.y -= kLabelWithInputTopPadding;

    // Due to fixed width, fields are guaranteed to not overlap.
    DCHECK_LE(NSMaxX(labelFrame), NSMinX(inputFieldFrame));
  }

  [spacer_ setFrame:spacerFrame];
  [label_ setFrame:labelFrame];
  [[self view] setFrameSize:preferredContainerSize];
}

@end
