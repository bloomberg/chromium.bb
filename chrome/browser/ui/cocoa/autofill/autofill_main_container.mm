// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_main_container.h"

#include <algorithm>
#include <cmath>

#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_details_container.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "grit/generated_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"

@interface AutofillMainContainer (Private)
- (void)buildWindowButtonsForFrame:(NSRect)frame;
- (void)layoutButtons;
@end

@implementation AutofillMainContainer

@synthesize target = target_;

- (id)initWithController:(autofill::AutofillDialogController*)controller {
  if (self = [super init]) {
    controller_ = controller;
  }
  return self;
}

- (void)loadView {
  [self buildWindowButtonsForFrame:NSZeroRect];

  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);
  [view setAutoresizesSubviews:YES];
  [view setSubviews:@[buttonContainer_]];
  [self setView:view];

  [self layoutButtons];

  detailsContainer_.reset(
      [[AutofillDetailsContainer alloc] initWithController:controller_]);
  NSSize frameSize = [[detailsContainer_ view] frame].size;
  [[detailsContainer_ view] setFrameOrigin:
      NSMakePoint(0, NSHeight([buttonContainer_ frame]))];
  frameSize.height += NSHeight([buttonContainer_ frame]);
  [[self view] setFrameSize:frameSize];
  [[self view] addSubview:[detailsContainer_ view]];
}

- (NSSize)preferredSize {
  // The buttons never change size, so rely on container.
  NSSize buttonSize = [buttonContainer_ frame].size;
  NSSize detailsSize = [detailsContainer_ preferredSize];

  NSSize size = NSMakeSize(std::max(buttonSize.width, detailsSize.width),
                           buttonSize.height + detailsSize.height);

  return size;
}

- (void)performLayout {
  // Assume that the frame for the container is set already.
  [detailsContainer_ performLayout];
}

- (void)buildWindowButtonsForFrame:(NSRect)frame {
  if (buttonContainer_.get())
    return;

  buttonContainer_.reset([[GTMWidthBasedTweaker alloc] initWithFrame:
      ui::kWindowSizeDeterminedLater]);
  [buttonContainer_
      setAutoresizingMask:(NSViewMinXMargin | NSViewMaxYMargin)];

  base::scoped_nsobject<NSButton> button(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [button setTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)];
  [button setKeyEquivalent:kKeyEquivalentEscape];
  [button setTarget:target_];
  [button setAction:@selector(cancel:)];
  [button sizeToFit];
  [buttonContainer_ addSubview:button];

  CGFloat nextX = NSMaxX([button frame]) + kButtonGap;
  button.reset([[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [button setFrameOrigin:NSMakePoint(nextX, 0)];
  [button  setTitle:l10n_util::GetNSStringWithFixup(
       IDS_AUTOFILL_DIALOG_SUBMIT_BUTTON)];
  [button setKeyEquivalent:kKeyEquivalentReturn];
  [button setTarget:target_];
  [button setAction:@selector(accept:)];
  [button sizeToFit];
  [buttonContainer_ addSubview:button];

  frame = NSMakeRect(
      NSWidth(frame) - NSMaxX([button frame]), 0,
      NSMaxX([button frame]), NSHeight([button frame]));

  [buttonContainer_ setFrame:frame];
}

- (void)layoutButtons {
  base::scoped_nsobject<GTMUILocalizerAndLayoutTweaker> layoutTweaker(
      [[GTMUILocalizerAndLayoutTweaker alloc] init]);
  [layoutTweaker tweakUI:buttonContainer_];
}

- (AutofillSectionContainer*)sectionForId:(autofill::DialogSection)section {
  return [detailsContainer_ sectionForId:section];
}

- (void)modelChanged {
  [detailsContainer_ modelChanged];
}

@end
