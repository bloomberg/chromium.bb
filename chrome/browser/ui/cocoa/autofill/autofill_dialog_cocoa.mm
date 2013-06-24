// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_cocoa.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_account_chooser.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_details_container.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_main_container.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_section_container.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_sign_in_container.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "ui/base/cocoa/flipped_view.h"
#include "ui/base/cocoa/window_size_constants.h"

namespace {

const CGFloat kAccountChooserHeight = 20.0;
const CGFloat kRelatedControlVerticalSpacing = 8.0;

}  // namespace;

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
  base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc]
          initWithCustomWindow:[sheet_controller_ window]]);
  constrained_window_.reset(
      new ConstrainedWindowMac(this, controller_->web_contents(), sheet));
}

// Closes the sheet and ends the modal loop. Triggers cleanup sequence.
void AutofillDialogCocoa::Hide() {
  constrained_window_->CloseWebContentsModalDialog();
}

void AutofillDialogCocoa::UpdateAccountChooser() {
  [sheet_controller_ updateAccountChooser];
}

void AutofillDialogCocoa::UpdateButtonStrip() {
}

void AutofillDialogCocoa::UpdateDetailArea() {
}

void AutofillDialogCocoa::UpdateForErrors() {
}

void AutofillDialogCocoa::UpdateNotificationArea() {
}

void AutofillDialogCocoa::UpdateAutocheckoutStepsArea() {
}

void AutofillDialogCocoa::UpdateSection(DialogSection section) {
  [sheet_controller_ updateSection:section];
}

void AutofillDialogCocoa::FillSection(DialogSection section,
                                      const DetailInput& originating_input) {
}

void AutofillDialogCocoa::GetUserInput(DialogSection section,
                                       DetailOutputMap* output) {
  [sheet_controller_ getInputs:output forSection:section];
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

void AutofillDialogCocoa::ModelChanged() {
  [sheet_controller_ modelChanged];
}

void AutofillDialogCocoa::OnSignInResize(const gfx::Size& pref_size) {
  // TODO(groby): Implement Mac support for this.
}

void AutofillDialogCocoa::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  constrained_window_.reset();
  // |this| belongs to |controller_|, so no self-destruction here.
  controller_->ViewClosed();
}

}  // autofill

#pragma mark Window Controller

@implementation AutofillDialogWindowController

- (id)initWithWebContents:(content::WebContents*)webContents
      autofillDialog:(autofill::AutofillDialogCocoa*)autofillDialog {
  DCHECK(webContents);

  base::scoped_nsobject<ConstrainedWindowCustomWindow> window(
      [[ConstrainedWindowCustomWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater]);

  if ((self = [super initWithWindow:window])) {
    webContents_ = webContents;
    autofillDialog_ = autofillDialog;

    mainContainer_.reset([[AutofillMainContainer alloc]
                             initWithController:autofillDialog->controller()]);
    [mainContainer_ setTarget:self];

    signInContainer_.reset(
        [[AutofillSignInContainer alloc]
            initWithController:autofillDialog->controller()]);
    [[signInContainer_ view] setHidden:YES];

    NSRect clientRect = [[mainContainer_ view] frame];
    clientRect.origin = NSMakePoint(chrome_style::kClientBottomPadding,
                                    chrome_style::kHorizontalPadding);
    [[mainContainer_ view] setFrame:clientRect];
    [[signInContainer_ view] setFrame:clientRect];

    NSRect headerRect = clientRect;
    headerRect.size.height = kAccountChooserHeight;
    headerRect.origin.y = NSMaxY(clientRect);
    accountChooser_.reset([[AutofillAccountChooser alloc]
                              initWithFrame:headerRect
                                 controller:autofillDialog->controller()]);

    // This needs a flipped content view because otherwise the size
    // animation looks odd. However, replacing the contentView for constrained
    // windows does not work - it does custom rendering.
    base::scoped_nsobject<NSView> flippedContentView(
        [[FlippedView alloc] initWithFrame:NSZeroRect]);
    [flippedContentView setSubviews:
        @[accountChooser_, [mainContainer_ view], [signInContainer_ view]]];
    [flippedContentView setAutoresizingMask:
        (NSViewWidthSizable | NSViewHeightSizable)];
    [[[self window] contentView] addSubview:flippedContentView];

    NSRect contentRect = clientRect;
    contentRect.origin = NSMakePoint(0, 0);
    contentRect.size.width += 2 * chrome_style::kHorizontalPadding;
    contentRect.size.height += NSHeight(headerRect) +
                               chrome_style::kClientBottomPadding +
                               chrome_style::kTitleTopPadding;
    [self performLayout];
  }
  return self;
}

- (void)requestRelayout {
  [self performLayout];
}

- (NSSize)preferredSize {
  NSSize contentSize;
  // TODO(groby): Currently, keep size identical to main container.
  // Change to allow autoresize of web contents.
  contentSize = [mainContainer_ preferredSize];

  NSSize headerSize = NSMakeSize(contentSize.width, kAccountChooserHeight);
  NSSize size = NSMakeSize(
      std::max(contentSize.width, headerSize.width),
      contentSize.height + headerSize.height + kRelatedControlVerticalSpacing);
  size.width += 2 * chrome_style::kHorizontalPadding;
  size.height += chrome_style::kClientBottomPadding +
                 chrome_style::kTitleTopPadding;
  return size;
}

- (void)performLayout {
  // Don't animate when we first show the window.
  BOOL shouldAnimate =
      !NSEqualRects(ui::kWindowSizeDeterminedLater, [[self window] frame]);

  NSRect contentRect = NSZeroRect;
  contentRect.size = [self preferredSize];
  NSRect clientRect = NSInsetRect(
      contentRect, chrome_style::kHorizontalPadding, 0);
  clientRect.origin.y = chrome_style::kClientBottomPadding;
  clientRect.size.height -= chrome_style::kTitleTopPadding +
                            chrome_style::kClientBottomPadding;

  NSRect headerRect, mainRect;
  NSDivideRect(clientRect, &headerRect, &mainRect,
               kAccountChooserHeight, NSMinYEdge);

  [accountChooser_ setFrame:headerRect];
  if ([[signInContainer_ view] isHidden]) {
    [[mainContainer_ view] setFrame:mainRect];
    [mainContainer_ performLayout];
  } else {
    [[signInContainer_ view] setFrame:mainRect];
  }

  NSRect frameRect = [[self window] frameRectForContentRect:contentRect];
  [[self window] setFrame:frameRect display:YES animate:shouldAnimate];
}

- (IBAction)accept:(id)sender {
  // TODO(groby): Validation goes here.
  autofillDialog_->controller()->OnAccept();
}

- (IBAction)cancel:(id)sender {
  // TODO(groby): Validation goes here.
  autofillDialog_->controller()->OnCancel();
  autofillDialog_->Hide();
}

- (void)updateAccountChooser {
  [accountChooser_ update];
}

- (void)updateSection:(autofill::DialogSection)section {
  [[mainContainer_ sectionForId:section] update];
}

- (content::NavigationController*)showSignIn {
  [signInContainer_ loadSignInPage];
  [[mainContainer_ view] setHidden:YES];
  [[signInContainer_ view] setHidden:NO];
  [self performLayout];

  return [signInContainer_ navigationController];
}

- (void)getInputs:(autofill::DetailOutputMap*)output
       forSection:(autofill::DialogSection)section {
  [[mainContainer_ sectionForId:section] getInputs:output];
}

- (void)hideSignIn {
  [[signInContainer_ view] setHidden:YES];
  [[mainContainer_ view] setHidden:NO];
  [self performLayout];
}

- (void)modelChanged {
  [mainContainer_ modelChanged];
}

@end
