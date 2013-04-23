// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_cocoa.h"

#include "chrome/browser/ui/chrome_style.h"
#include "base/mac/bundle_locations.h"
#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_main_container.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_sign_in_container.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"

namespace autofill {

// static
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogController* controller) {
  return new AutofillDialogCocoa(controller);
}

AutofillDialogCocoa::AutofillDialogCocoa(AutofillDialogController* controller)
    : controller_(controller) {
}

AutofillDialogCocoa::~AutofillDialogCocoa() {
}

void AutofillDialogCocoa::Show() {
  // This should only be called once.
  DCHECK(!sheet_controller_.get());
  sheet_controller_.reset([[AutofillDialogWindowController alloc]
       initWithWebContents:controller_->web_contents()
            autofillDialog:this]);
  scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc]
          initWithCustomWindow:[sheet_controller_ window]]);
  constrained_window_.reset(
      new ConstrainedWindowMac(this, controller_->web_contents(), sheet));
}

void AutofillDialogCocoa::Hide() {
}

void AutofillDialogCocoa::UpdateAccountChooser() {
  [sheet_controller_ updateAccountChooser];
}

void AutofillDialogCocoa::UpdateButtonStrip() {
}

void AutofillDialogCocoa::UpdateNotificationArea() {
}

void AutofillDialogCocoa::UpdateSection(DialogSection section) {
}

void AutofillDialogCocoa::FillSection(DialogSection section,
                                      const DetailInput& originating_input) {
}

void AutofillDialogCocoa::GetUserInput(DialogSection section,
                                       DetailOutputMap* output) {
}

string16 AutofillDialogCocoa::GetCvc() {
  return string16();
}

bool AutofillDialogCocoa::SaveDetailsLocally() {
  return false;
}

const content::NavigationController* AutofillDialogCocoa::ShowSignIn() {
  return [sheet_controller_ showSignIn];
}

void AutofillDialogCocoa::HideSignIn() {
  [sheet_controller_ hideSignIn];
}

void AutofillDialogCocoa::UpdateProgressBar(double value) {}

void AutofillDialogCocoa::ModelChanged() {}

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
    mainContainer_.reset([[AutofillMainContainer alloc]
                             initWithController:autofillDialog->controller()]);
    [mainContainer_ setTarget:self];
    [[mainContainer_ view] setFrame:clientFrame];

    signInContainer_.reset(
        [[AutofillSignInContainer alloc]
            initWithController:autofillDialog->controller()]);
    [[signInContainer_ view] setHidden:YES];
    [[signInContainer_ view] setFrame:clientFrame];

    [[[self window] contentView] setSubviews:
        @[[mainContainer_ view], [signInContainer_ view]]];
  }
  return self;
}

- (void)updateAccountChooser {
  [[mainContainer_ accountChooser] update];
}

- (content::NavigationController*)showSignIn {
  [signInContainer_ loadSignInPage];
  [[mainContainer_ view] setHidden:YES];
  [[signInContainer_ view] setHidden:NO];

  return [signInContainer_ navigationController];
}

- (void)hideSignIn {
  [[signInContainer_ view] setHidden:YES];
  [[mainContainer_ view] setHidden:NO];
}


- (IBAction)closeSheet:(id)sender {
  autofillDialog_->PerformClose();
}

@end
