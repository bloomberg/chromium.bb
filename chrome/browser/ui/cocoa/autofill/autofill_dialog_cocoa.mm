// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_cocoa.h"

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#include "chrome/browser/ui/chrome_style.h"
#include "base/mac/bundle_locations.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_account_chooser.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "grit/generated_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

// static
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogController* controller) {
  return new AutofillDialogCocoa(controller);
}

AutofillDialogCocoa::AutofillDialogCocoa(AutofillDialogController* controller)
    : controller_(controller) {
  sheet_controller_.reset([[AutofillDialogWindowController alloc]
       initWithWebContents:controller_->web_contents()
            autofillDialog:this]);
  scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc]
          initWithCustomWindow:[sheet_controller_ window]]);
  constrained_window_.reset(
      new ConstrainedWindowMac(this, controller_->web_contents(), sheet));
}

AutofillDialogCocoa::~AutofillDialogCocoa() {
}

void AutofillDialogCocoa::Show() {
}

void AutofillDialogCocoa::Hide() {
}

void AutofillDialogCocoa::UpdateAccountChooser() {
}

void AutofillDialogCocoa::UpdateButtonStrip() {
}

void AutofillDialogCocoa::UpdateNotificationArea() {
}

void AutofillDialogCocoa::UpdateSection(DialogSection section,
                                        UserInputAction action) {
}

void AutofillDialogCocoa::GetUserInput(DialogSection section,
                                       DetailOutputMap* output) {
}

string16 AutofillDialogCocoa::GetCvc() {
  return string16();
}

bool AutofillDialogCocoa::UseBillingForShipping() {
  return false;
}

bool AutofillDialogCocoa::SaveDetailsLocally() {
  return false;
}

const content::NavigationController* AutofillDialogCocoa::ShowSignIn() {
  return NULL;
}

void AutofillDialogCocoa::HideSignIn() {}

void AutofillDialogCocoa::UpdateProgressBar(double value) {}

void AutofillDialogCocoa::ModelChanged() {}

void AutofillDialogCocoa::SubmitForTesting() {}

void AutofillDialogCocoa::CancelForTesting() {
  PerformClose();
}

void AutofillDialogCocoa::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  constrained_window_.reset();
  // |this| belongs to |controller_|, so no self-destruction here.
  controller_->ViewClosed();
}

void AutofillDialogCocoa::PerformClose() {
  controller_->OnCancel();
  constrained_window_->CloseWebContentsModalDialog();
}

}  // autofill

#pragma mark Window Controller

@implementation AutofillDialogWindowController

- (id)initWithWebContents:(content::WebContents*)webContents
      autofillDialog:(autofill::AutofillDialogCocoa*)autofillDialog {
  DCHECK(webContents);

  // TODO(groby): Should be ui::kWindowSizeDeterminedLater
  NSRect frame = NSMakeRect(0, 0, 550, 600);
  scoped_nsobject<ConstrainedWindowCustomWindow> window(
      [[ConstrainedWindowCustomWindow alloc] initWithContentRect:frame]);

  if ((self = [super initWithWindow:window])) {
    webContents_ = webContents;
    autofillDialog_ = autofillDialog;

    NSRect clientFrame = NSInsetRect(
        frame, chrome_style::kHorizontalPadding, 0);
    clientFrame.size.height -= chrome_style::kTitleTopPadding +
                               chrome_style::kClientBottomPadding;
    clientFrame.origin.y = chrome_style::kClientBottomPadding;

    const CGFloat kAccountChooserHeight = 20.0;
    NSRect accountChooserFrame = NSMakeRect(
        clientFrame.origin.x, NSMaxY(clientFrame) - kAccountChooserHeight,
        clientFrame.size.width, kAccountChooserHeight);
    accountChooser_.reset([[AutofillAccountChooser alloc]
                               initWithFrame:accountChooserFrame
                                  controller:autofillDialog->controller()]);

    [[[self window] contentView] addSubview:accountChooser_];

    [self buildWindowButtons];
    [self layoutButtons];
  }
  return self;
}

- (void)updateAccountChooser {
  [accountChooser_ update];
}

- (IBAction)closeSheet:(id)sender {
  autofillDialog_->PerformClose();
}

- (void)buildWindowButtons {
  if (buttonContainer_.get())
    return;

  buttonContainer_.reset([[GTMWidthBasedTweaker alloc] initWithFrame:
      ui::kWindowSizeDeterminedLater]);
  [buttonContainer_
      setAutoresizingMask:(NSViewMinXMargin | NSViewMinYMargin)];

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

  const CGFloat dialogOffset = NSWidth([[self window] frame]) -
      chrome_style::kHorizontalPadding - NSMaxX([button frame]);
  [buttonContainer_ setFrame:
      NSMakeRect(dialogOffset, chrome_style::kClientBottomPadding,
                 NSMaxX([button frame]), NSMaxY([button frame]))];

  [[[self window] contentView] addSubview:buttonContainer_];
}

- (void)layoutButtons {
  scoped_nsobject<GTMUILocalizerAndLayoutTweaker> layoutTweaker(
      [[GTMUILocalizerAndLayoutTweaker alloc] init]);
  [layoutTweaker tweakUI:buttonContainer_];
}

@end