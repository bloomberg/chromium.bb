// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_details_container.h"

#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_section_container.h"

@implementation AutofillDetailsContainer

- (id)initWithController:(autofill::AutofillDialogController*)controller {
  if (self = [super init]) {
    controller_ = controller;
  }
  return self;
}

- (void)addSection:(autofill::DialogSection)section {
  scoped_nsobject<AutofillSectionContainer> sectionContainer(
      [[AutofillSectionContainer alloc] initWithController:controller_
                                                forSection:section]);
  [details_ addObject:sectionContainer];
}

- (void)loadView {
  details_.reset([[NSMutableArray alloc] init]);

  [self addSection:autofill::SECTION_EMAIL];
  [self addSection:autofill::SECTION_CC];
  [self addSection:autofill::SECTION_BILLING];
  // TODO(groby): Add SECTION_CC_BILLING once toggling is enabled.
  [self addSection:autofill::SECTION_SHIPPING];

  [self setView:[[NSView alloc] init]];

  NSRect rect = NSZeroRect;
  for (AutofillSectionContainer* container in
      [details_ reverseObjectEnumerator]) {
    [[container view] setFrameOrigin:NSMakePoint(0, NSMaxY(rect))];
    rect = NSUnionRect(rect, [[container view] frame]);
    [[self view] addSubview:[container view]];
  }
  [[self view] setFrame:rect];
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

@end
