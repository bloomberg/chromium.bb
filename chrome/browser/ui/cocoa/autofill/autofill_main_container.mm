// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_main_container.h"

#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_account_chooser.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "grit/generated_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"

@interface AutofillMainContainer (Private)
- (void)buildWindowButtonsForFrame:(NSRect)frame;
- (void)layoutButtons;
- (void)closeSheet:(id)sender;
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
  const CGFloat kAccountChooserHeight = 20.0;
  NSRect accountChooserFrame = NSMakeRect(
      0, -kAccountChooserHeight,
      0, kAccountChooserHeight);
  accountChooser_.reset([[AutofillAccountChooser alloc]
                            initWithFrame:accountChooserFrame
                                controller:controller_]);
  [accountChooser_ setAutoresizingMask:(NSViewWidthSizable | NSViewMinYMargin)];

  [self buildWindowButtonsForFrame:NSZeroRect];

  scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);
  [view setAutoresizesSubviews:YES];
  [view setSubviews:@[accountChooser_, buttonContainer_]];
  self.view = view;

  [self layoutButtons];
}

- (void)buildWindowButtonsForFrame:(NSRect)frame {
  if (buttonContainer_.get())
    return;

  buttonContainer_.reset([[GTMWidthBasedTweaker alloc] initWithFrame:
      ui::kWindowSizeDeterminedLater]);
  [buttonContainer_
      setAutoresizingMask:(NSViewMinXMargin | NSViewMaxYMargin)];

  scoped_nsobject<NSButton> button(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [button setTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)];
  [button setKeyEquivalent:kKeyEquivalentEscape];
  [button setTarget:self];
  [button setAction:@selector(closeSheet:)];
  [button sizeToFit];
  [buttonContainer_ addSubview:button];

  CGFloat nextX = NSMaxX([button frame]) + kButtonGap;
  button.reset([[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [button setFrameOrigin:NSMakePoint(nextX, 0)];
  [button  setTitle:l10n_util::GetNSStringWithFixup(
       IDS_AUTOFILL_DIALOG_SUBMIT_BUTTON)];
  [button setKeyEquivalent:kKeyEquivalentReturn];
  [button setTarget:self];
  [button setAction:@selector(closeSheet:)];
  [button sizeToFit];
  [buttonContainer_ addSubview:button];

  frame = NSMakeRect(
      NSWidth(frame) - NSMaxX([button frame]), 0,
      NSMaxX([button frame]), NSHeight([button frame]));

  [buttonContainer_ setFrame:frame];
}

- (void)layoutButtons {
  scoped_nsobject<GTMUILocalizerAndLayoutTweaker> layoutTweaker(
      [[GTMUILocalizerAndLayoutTweaker alloc] init]);
  [layoutTweaker tweakUI:buttonContainer_];
}

- (AutofillAccountChooser*)accountChooser {
  return accountChooser_;
}

- (void)closeSheet:(id)sender {
  [target_ closeSheet:sender];
}


@end

