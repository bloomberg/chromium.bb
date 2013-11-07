// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_details_container.h"

#include <algorithm>

#include "base/mac/foundation_util.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_error_bubble_controller.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_section_container.h"

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
  bool allValid = true;
  for (AutofillSectionContainer* details in details_.get()) {
    if (![[details view] isHidden])
      allValid = [details validateFor:autofill::VALIDATE_FINAL] && allValid;
  }
  return allValid;
}

- (void)updateErrorBubble {
  if (!delegate_->ShouldShowErrorBubble()) {
    [errorBubbleController_ close];
  }
}

- (NSPoint)anchorPointFromView:(NSView*)view {
  // All math done in window coordinates, since views might be flipped.
  NSRect viewRect = [view convertRect:[view bounds] toView:nil];
  NSPoint anchorPoint =
      NSMakePoint(NSMidX(viewRect), NSMinY(viewRect));
  return [[view window] convertBaseToScreen:anchorPoint];
}

- (void)errorBubbleWindowWillClose:(NSNotification*)notification {
  DCHECK_EQ([notification object], [errorBubbleController_ window]);

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center removeObserver:self
                    name:NSWindowWillCloseNotification
                  object:[errorBubbleController_ window]];
  errorBubbleController_ = nil;
}

- (void)showErrorBubbleForField:(NSControl<AutofillInputField>*)field {
  DCHECK(!errorBubbleController_);
  NSWindow* parentWindow = [field window];
  DCHECK(parentWindow);
  errorBubbleController_ =
        [[AutofillErrorBubbleController alloc]
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
  NSPoint anchorPoint = NSMakePoint(NSMidX(viewRect), NSMinY(viewRect));
  [errorBubbleController_ setAnchorPoint:
      [parentWindow convertBaseToScreen:anchorPoint]];

  [errorBubbleController_ showWindow:self];
}

- (void)hideErrorBubble {
  [errorBubble_ setHidden:YES];
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

@end
