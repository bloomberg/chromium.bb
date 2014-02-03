// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_details_container.h"

#include <algorithm>

#include "base/mac/foundation_util.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_bubble_controller.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_section_container.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"

typedef BOOL (^FieldFilterBlock)(NSView<AutofillInputField>*);

@interface AutofillDetailsContainer ()

// Find the editable input field that is closest to the top of the dialog and
// matches the |predicateBlock|.
- (NSView*)firstEditableFieldMatchingBlock:(FieldFilterBlock)predicateBlock;

@end

@implementation AutofillDetailsContainer

- (id)initWithDelegate:(autofill::AutofillDialogViewDelegate*)delegate {
  if (self = [super init]) {
    delegate_ = delegate;
  }
  return self;
}

- (void)addSection:(autofill::DialogSection)section {
  base::scoped_nsobject<AutofillSectionContainer> sectionContainer(
      [[AutofillSectionContainer alloc] initWithDelegate:delegate_
                                                forSection:section]);
  [sectionContainer setValidationDelegate:self];
  [details_ addObject:sectionContainer];
}

- (void)loadView {
  details_.reset([[NSMutableArray alloc] init]);

  [self addSection:autofill::SECTION_CC];
  [self addSection:autofill::SECTION_BILLING];
  [self addSection:autofill::SECTION_CC_BILLING];
  [self addSection:autofill::SECTION_SHIPPING];

  scrollView_.reset([[NSScrollView alloc] initWithFrame:NSZeroRect]);
  [scrollView_ setHasVerticalScroller:YES];
  [scrollView_ setHasHorizontalScroller:NO];
  [scrollView_ setBorderType:NSNoBorder];
  [scrollView_ setAutohidesScrollers:YES];
  [self setView:scrollView_];

  [scrollView_ setDocumentView:[[NSView alloc] initWithFrame:NSZeroRect]];

  for (AutofillSectionContainer* container in details_.get())
    [[scrollView_ documentView] addSubview:[container view]];

  [self performLayout];
}

- (NSSize)preferredSize {
  NSSize size = NSZeroSize;
  for (AutofillSectionContainer* container in details_.get()) {
    NSSize containerSize = [container preferredSize];
    size.height += containerSize.height;
    size.width = std::max(containerSize.width, size.width);
  }
  return size;
}

- (void)performLayout {
  NSRect rect = NSZeroRect;
  for (AutofillSectionContainer* container in
      [details_ reverseObjectEnumerator]) {
    if (![[container view] isHidden]) {
      [container performLayout];
      [[container view] setFrameOrigin:NSMakePoint(0, NSMaxY(rect))];
      rect = NSUnionRect(rect, [[container view] frame]);
    }
  }

  [[scrollView_ documentView] setFrameSize:[self preferredSize]];
}

- (AutofillSectionContainer*)sectionForId:(autofill::DialogSection)section {
  for (AutofillSectionContainer* details in details_.get()) {
    if ([details section] == section)
      return details;
  }
  return nil;
}

- (void)modelChanged {
  for (AutofillSectionContainer* details in details_.get())
    [details modelChanged];
}

- (BOOL)validate {
  // Account for a subtle timing issue. -validate is called from the dialog's
  // -accept. -accept then hides the dialog. If the data does not validate the
  // dialog is then reshown, focusing on the first invalid field. This happens
  // without running the message loop, so windowWillClose has not fired when
  // the dialog and error bubble is reshown, leading to a missing error bubble.
  // Resetting the anchor view here forces the bubble to show.
  errorBubbleAnchorView_ = nil;

  bool allValid = true;
  for (AutofillSectionContainer* details in details_.get()) {
    if (![[details view] isHidden])
      allValid = [details validateFor:autofill::VALIDATE_FINAL] && allValid;
  }
  return allValid;
}

- (NSView*)firstInvalidField {
  return [self firstEditableFieldMatchingBlock:
      ^BOOL (NSView<AutofillInputField>* field) {
          return [field invalid];
      }];
}

- (NSView*)firstVisibleField {
  return [self firstEditableFieldMatchingBlock:
      ^BOOL (NSView<AutofillInputField>* field) {
          return YES;
      }];
}

- (void)scrollToView:(NSView*)field {
  const CGFloat bottomPadding = 5.0;  // Padding below the visible field.

  NSClipView* clipView = [scrollView_ contentView];
  NSRect fieldRect = [field convertRect:[field bounds] toView:clipView];

  // If the entire field is already visible, let's not scroll.
  NSRect documentRect = [clipView documentVisibleRect];
  documentRect = [[clipView documentView] convertRect:documentRect
                                               toView:clipView];
  if (NSContainsRect(documentRect, fieldRect))
    return;

  NSPoint scrollPoint = [clipView constrainScrollPoint:
      NSMakePoint(0, NSMinY(fieldRect) - bottomPadding)];
  [clipView scrollToPoint:scrollPoint];
  [scrollView_ reflectScrolledClipView:clipView];
  [self updateErrorBubble];
}

- (void)updateErrorBubble {
  if (!delegate_->ShouldShowErrorBubble()) {
    [errorBubbleController_ close];
  }
}

- (void)errorBubbleWindowWillClose:(NSNotification*)notification {
  DCHECK_EQ([notification object], [errorBubbleController_ window]);

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center removeObserver:self
                    name:NSWindowWillCloseNotification
                  object:[errorBubbleController_ window]];
  errorBubbleController_ = nil;
  errorBubbleAnchorView_ = nil;
}

- (void)showErrorBubbleForField:(NSControl<AutofillInputField>*)field {
  // If there is already a bubble controller handling this field, reuse.
  if (errorBubbleController_ && errorBubbleAnchorView_ == field) {
    [errorBubbleController_ setMessage:[field validityMessage]];

    return;
  }

  if (errorBubbleController_)
    [errorBubbleController_ close];
  DCHECK(!errorBubbleController_);
  NSWindow* parentWindow = [field window];
  DCHECK(parentWindow);
  errorBubbleController_ =
        [[AutofillBubbleController alloc]
            initWithParentWindow:parentWindow
                         message:[field validityMessage]];

  // Handle bubble self-deleting.
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(errorBubbleWindowWillClose:)
                 name:NSWindowWillCloseNotification
               object:[errorBubbleController_ window]];

  // Compute anchor point (in window coords - views might be flipped).
  NSRect viewRect = [field convertRect:[field bounds] toView:nil];

  // If a bubble at maximum size with a left-aligned edge would exceed the
  // window width, align the right edge of bubble and view. In all other
  // cases, align the left edge of the bubble and the view.
  // Alignment is based on maximum width to avoid the arrow changing positions
  // if the validation bubble stays on the same field but gets a message of
  // differing length. (E.g. "Field is required"/"Invalid Zip Code. Please
  // check and try again" if an empty zip field gets changed to a bad zip).
  NSPoint anchorPoint;
  if ((NSMinX(viewRect) + [errorBubbleController_ maxWidth]) >
      NSWidth([parentWindow frame])) {
    anchorPoint = NSMakePoint(NSMaxX(viewRect), NSMinY(viewRect));
    [[errorBubbleController_ bubble] setArrowLocation:info_bubble::kTopRight];
    [[errorBubbleController_ bubble] setAlignment:
        info_bubble::kAlignRightEdgeToAnchorEdge];

  } else {
    anchorPoint = NSMakePoint(NSMinX(viewRect), NSMinY(viewRect));
    [[errorBubbleController_ bubble] setArrowLocation:info_bubble::kTopLeft];
    [[errorBubbleController_ bubble] setAlignment:
        info_bubble::kAlignLeftEdgeToAnchorEdge];
  }
  [errorBubbleController_ setAnchorPoint:
      [parentWindow convertBaseToScreen:anchorPoint]];

  errorBubbleAnchorView_ = field;
  [errorBubbleController_ showWindow:self];
}

- (void)hideErrorBubble {
  [errorBubbleController_ close];
}

- (void)updateMessageForField:(NSControl<AutofillInputField>*)field {
  // Ignore fields that are not first responder. Testing this is a bit
  // convoluted, since for NSTextFields with firstResponder status, the
  // firstResponder is a subview of the NSTextField, not the field itself.
  NSView* firstResponderView =
      base::mac::ObjCCast<NSView>([[field window] firstResponder]);
  if (![firstResponderView isDescendantOf:field])
    return;
  if (!delegate_->ShouldShowErrorBubble()) {
    DCHECK(!errorBubbleController_);
    return;
  }

  if ([field invalid]) {
    [self showErrorBubbleForField:field];
  } else {
    [errorBubbleController_ close];
  }
}

- (NSView*)firstEditableFieldMatchingBlock:(FieldFilterBlock)predicateBlock {
  base::scoped_nsobject<NSMutableArray> fields([[NSMutableArray alloc] init]);

  for (AutofillSectionContainer* details in details_.get()) {
    if (![[details view] isHidden])
      [details addInputsToArray:fields];
  }

  NSPoint selectedFieldOrigin = NSZeroPoint;
  NSView* selectedField = nil;
  for (NSControl<AutofillInputField>* field in fields.get()) {
    if (!base::mac::ObjCCast<NSControl>(field))
      continue;
    if (![field conformsToProtocol:@protocol(AutofillInputField)])
      continue;
    if ([field isHiddenOrHasHiddenAncestor])
      continue;
    if (![field isEnabled])
      continue;
    if (![field canBecomeKeyView])
      continue;
    if (!predicateBlock(field))
      continue;

    NSPoint fieldOrigin = [field convertPoint:[field bounds].origin toView:nil];
    if (fieldOrigin.y < selectedFieldOrigin.y)
      continue;
    if (fieldOrigin.y == selectedFieldOrigin.y &&
        fieldOrigin.x > selectedFieldOrigin.x) {
      continue;
    }

    selectedField = field;
    selectedFieldOrigin = fieldOrigin;
  }

  return selectedField;
}

@end
