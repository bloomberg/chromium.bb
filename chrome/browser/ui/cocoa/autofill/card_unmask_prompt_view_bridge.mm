// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_controller.h"
#include "chrome/browser/ui/cocoa/autofill/card_unmask_prompt_view_bridge.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_pop_up_button.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_control_utils.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const CGFloat kButtonGap = 6.0f;
const CGFloat kDialogContentMinWidth = 210.0f;
const CGFloat kCvcInputWidth = 64.0f;

}  // namespace

namespace autofill {

// static
CardUnmaskPromptView* CardUnmaskPromptView::CreateAndShow(
    CardUnmaskPromptController* controller) {
  return new CardUnmaskPromptViewBridge(controller);
}

#pragma mark CardUnmaskPromptViewBridge

CardUnmaskPromptViewBridge::CardUnmaskPromptViewBridge(
    CardUnmaskPromptController* controller)
    : controller_(controller) {
  view_controller_.reset(
      [[CardUnmaskPromptViewCocoa alloc] initWithBridge:this]);

  // Setup the constrained window that will show the view.
  base::scoped_nsobject<NSWindow> window([[ConstrainedWindowCustomWindow alloc]
      initWithContentRect:[[view_controller_ view] bounds]]);
  [window setContentView:[view_controller_ view]];
  base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc] initWithCustomWindow:window]);
  constrained_window_.reset(
      new ConstrainedWindowMac(this, controller_->GetWebContents(), sheet));
}

CardUnmaskPromptViewBridge::~CardUnmaskPromptViewBridge() {
}

void CardUnmaskPromptViewBridge::ControllerGone() {
  controller_ = nullptr;
  PerformClose();
}

void CardUnmaskPromptViewBridge::DisableAndWaitForVerification() {
}

void CardUnmaskPromptViewBridge::GotVerificationResult(
    const base::string16& error_message,
    bool allow_retry) {
}

void CardUnmaskPromptViewBridge::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  constrained_window_.reset();
  if (controller_)
    controller_->OnUnmaskDialogClosed();
}

CardUnmaskPromptController* CardUnmaskPromptViewBridge::GetController() {
  return controller_;
}

void CardUnmaskPromptViewBridge::PerformClose() {
  constrained_window_->CloseWebContentsModalDialog();
}

}  // autofill

#pragma mark CardUnmaskPromptViewCocoa

@implementation CardUnmaskPromptViewCocoa

+ (AutofillPopUpButton*)buildDatePopupWithModel:(ui::ComboboxModel&)model {
  AutofillPopUpButton* popup =
      [[AutofillPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];

  for (int i = 0; i < model.GetItemCount(); ++i) {
    [popup addItemWithTitle:base::SysUTF16ToNSString(model.GetItemAt(i))];
  }
  [popup setDefaultValue:base::SysUTF16ToNSString(
                             model.GetItemAt(model.GetDefaultIndex()))];
  [popup sizeToFit];
  return popup;
}

// Set |view|'s frame to the minimum dimensions required to contain all of its
// subviews.
+ (void)sizeToFitView:(NSView*)view {
  NSRect frame = NSZeroRect;
  for (NSView* child in [view subviews]) {
    frame = NSUnionRect(frame, [child frame]);
  }
  [view setFrame:frame];
}

+ (void)verticallyCenterSubviewsInView:(NSView*)view {
  CGFloat height = NSHeight([view frame]);
  for (NSView* child in [view subviews]) {
    [child setFrameOrigin:NSMakePoint(
                              NSMinX([child frame]),
                              ceil((height - NSHeight([child frame])) * 0.5))];
  }
}

- (id)initWithBridge:(autofill::CardUnmaskPromptViewBridge*)bridge {
  DCHECK(bridge);

  if ((self = [super initWithNibName:nil bundle:nil]))
    bridge_ = bridge;

  return self;
}

- (IBAction)closeSheet:(id)sender {
  bridge_->PerformClose();
}

- (void)loadView {
  autofill::CardUnmaskPromptController* controller = bridge_->GetController();
  DCHECK(controller);

  base::scoped_nsobject<NSBox> mainView(
      [[NSBox alloc] initWithFrame:NSZeroRect]);
  [mainView setBoxType:NSBoxCustom];
  [mainView setBorderType:NSNoBorder];
  [mainView setTitlePosition:NSNoTitle];
  [mainView
      setContentViewMargins:NSMakeSize(chrome_style::kHorizontalPadding, 0)];

  base::scoped_nsobject<NSView> inputRowView(
      [[NSView alloc] initWithFrame:NSZeroRect]);
  [mainView addSubview:inputRowView];

  // Title label.
  NSTextField* title = constrained_window::CreateLabel();
  NSAttributedString* titleString =
      constrained_window::GetAttributedLabelString(
          SysUTF16ToNSString(controller->GetWindowTitle()),
          chrome_style::kTitleFontStyle, NSNaturalTextAlignment,
          NSLineBreakByWordWrapping);
  [title setAttributedStringValue:titleString];
  [title sizeToFit];
  [mainView addSubview:title];

  // Instructions label.
  NSTextField* instructions = constrained_window::CreateLabel();
  NSAttributedString* instructionsString =
      constrained_window::GetAttributedLabelString(
          SysUTF16ToNSString(controller->GetInstructionsMessage()),
          chrome_style::kTextFontStyle, NSNaturalTextAlignment,
          NSLineBreakByWordWrapping);
  [instructions setAttributedStringValue:instructionsString];
  [mainView addSubview:instructions];

  // Expiration date.
  base::scoped_nsobject<NSView> expirationView;
  if (controller->ShouldRequestExpirationDate()) {
    expirationView.reset([[NSView alloc] initWithFrame:NSZeroRect]);

    // Month.
    autofill::MonthComboboxModel monthModel;
    base::scoped_nsobject<AutofillPopUpButton> monthPopup(
        [CardUnmaskPromptViewCocoa buildDatePopupWithModel:monthModel]);
    [expirationView addSubview:monthPopup];

    // Year.
    autofill::YearComboboxModel yearModel;
    base::scoped_nsobject<AutofillPopUpButton> yearPopup(
        [CardUnmaskPromptViewCocoa buildDatePopupWithModel:yearModel]);
    [expirationView addSubview:yearPopup];

    // Layout month and year within expirationView.
    [yearPopup
        setFrameOrigin:NSMakePoint(NSMaxX([monthPopup frame]) + kButtonGap, 0)];
    NSRect expirationFrame = NSUnionRect([monthPopup frame], [yearPopup frame]);
    expirationFrame.size.width += kButtonGap;
    [expirationView setFrame:expirationFrame];
    [inputRowView addSubview:expirationView];
  }

  // CVC text input.
  base::scoped_nsobject<NSTextField> cvcInput(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [[cvcInput cell]
      setPlaceholderString:l10n_util::GetNSString(
                               IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC)];
  [[cvcInput cell] setScrollable:YES];
  [cvcInput sizeToFit];
  [cvcInput setFrame:NSMakeRect(NSMaxX([expirationView frame]), 0,
                                kCvcInputWidth, NSHeight([cvcInput frame]))];
  [inputRowView addSubview:cvcInput];

  // CVC image.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NSImage* cvcImage =
      rb.GetNativeImageNamed(controller->GetCvcImageRid()).ToNSImage();
  base::scoped_nsobject<NSImageView> cvcImageView(
      [[NSImageView alloc] initWithFrame:NSZeroRect]);
  [cvcImageView setImage:cvcImage];
  [cvcImageView setFrameSize:[cvcImage size]];
  [cvcImageView
      setFrameOrigin:NSMakePoint(NSMaxX([cvcInput frame]) + kButtonGap, 0)];
  [inputRowView addSubview:cvcImageView];

  // Cancel button.
  base::scoped_nsobject<NSButton> cancelButton(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [cancelButton setTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)];
  [cancelButton setKeyEquivalent:kKeyEquivalentEscape];
  [cancelButton setTarget:self];
  [cancelButton setAction:@selector(closeSheet:)];
  [cancelButton sizeToFit];
  [mainView addSubview:cancelButton];

  // Verify button.
  base::scoped_nsobject<NSButton> verifyButton(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  // TODO(bondd): use l10n string.
  [verifyButton setTitle:@"Verify"];
  [verifyButton setKeyEquivalent:kKeyEquivalentReturn];
  [verifyButton setTarget:self];
  [verifyButton setAction:@selector(closeSheet:)];
  [verifyButton sizeToFit];
  [mainView addSubview:verifyButton];

  // Layout inputRowView.
  [CardUnmaskPromptViewCocoa sizeToFitView:inputRowView];
  [CardUnmaskPromptViewCocoa verticallyCenterSubviewsInView:inputRowView];

  // Calculate dialog content width.
  CGFloat contentWidth =
      std::max(NSWidth([title frame]), NSWidth([inputRowView frame]));
  contentWidth = std::max(contentWidth, kDialogContentMinWidth);

  // Layout mainView contents, starting at the bottom and moving up.

  // Verify and Cancel buttons.
  [verifyButton
      setFrameOrigin:NSMakePoint(contentWidth - NSWidth([verifyButton frame]),
                                 chrome_style::kClientBottomPadding)];

  [cancelButton
      setFrameOrigin:NSMakePoint(NSMinX([verifyButton frame]) - kButtonGap -
                                     NSWidth([cancelButton frame]),
                                 NSMinY([verifyButton frame]))];

  // Input row.
  [inputRowView setFrameOrigin:NSMakePoint(0, NSMaxY([cancelButton frame]) +
                                                  chrome_style::kRowPadding)];

  // Instruction label.
  [instructions setFrameOrigin:NSMakePoint(0, NSMaxY([inputRowView frame]) +
                                                  chrome_style::kRowPadding)];
  NSSize instructionsSize = [[instructions cell]
      cellSizeForBounds:NSMakeRect(0, 0, contentWidth, CGFLOAT_MAX)];
  [instructions setFrameSize:instructionsSize];

  // Title label.
  [title setFrameOrigin:NSMakePoint(0, NSMaxY([instructions frame]) +
                                           chrome_style::kRowPadding)];

  // Dialog size.
  [mainView
      setFrameSize:NSMakeSize(
                       contentWidth + [mainView contentViewMargins].width * 2.0,
                       NSMaxY([title frame]) + chrome_style::kTitleTopPadding)];

  [self setView:mainView];
}

@end
