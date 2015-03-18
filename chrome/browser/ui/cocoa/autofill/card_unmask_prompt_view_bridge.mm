// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_controller.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_tooltip_controller.h"
#include "chrome/browser/ui/cocoa/autofill/card_unmask_prompt_view_bridge.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_control_utils.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
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
  [view_controller_ setInputsEnabled:false];
  [view_controller_ updateVerifyButtonEnabled];
}

void CardUnmaskPromptViewBridge::GotVerificationResult(
    const base::string16& error_message,
    bool allow_retry) {
  [view_controller_ setInputsEnabled:true];
  [view_controller_ updateVerifyButtonEnabled];
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

@implementation CardUnmaskPromptViewCocoa {
  base::scoped_nsobject<NSTextField> cvcInput_;
  base::scoped_nsobject<NSPopUpButton> monthPopup_;
  base::scoped_nsobject<NSPopUpButton> yearPopup_;
  base::scoped_nsobject<NSButton> verifyButton_;
  base::scoped_nsobject<NSButton> storageCheckbox_;
  base::scoped_nsobject<AutofillTooltipController> storageTooltip_;

  int monthPopupDefaultIndex_;
  int yearPopupDefaultIndex_;

  // Owns |self|.
  autofill::CardUnmaskPromptViewBridge* bridge_;
}

+ (NSPopUpButton*)buildDatePopupWithModel:(ui::ComboboxModel&)model {
  NSPopUpButton* popup =
      [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];

  for (int i = 0; i < model.GetItemCount(); ++i) {
    [popup addItemWithTitle:base::SysUTF16ToNSString(model.GetItemAt(i))];
  }
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

- (void)setInputsEnabled:(BOOL)enabled {
  [cvcInput_ setEnabled:enabled];
  [monthPopup_ setEnabled:enabled];
  [yearPopup_ setEnabled:enabled];
}

- (void)updateVerifyButtonEnabled {
  autofill::CardUnmaskPromptController* controller = bridge_->GetController();
  DCHECK(controller);

  BOOL enable =
      [cvcInput_ isEnabled] &&
      controller->InputCvcIsValid(
          base::SysNSStringToUTF16([cvcInput_ stringValue])) &&
      (!monthPopup_ ||
       [monthPopup_ indexOfSelectedItem] != monthPopupDefaultIndex_) &&
      (!yearPopup_ ||
       [yearPopup_ indexOfSelectedItem] != yearPopupDefaultIndex_);

  [verifyButton_ setEnabled:enable];
}

- (void)onVerify:(id)sender {
  autofill::CardUnmaskPromptController* controller = bridge_->GetController();
  DCHECK(controller);

  controller->OnUnmaskResponse(
      base::SysNSStringToUTF16([cvcInput_ stringValue]),
      base::SysNSStringToUTF16([monthPopup_ titleOfSelectedItem]),
      base::SysNSStringToUTF16([yearPopup_ titleOfSelectedItem]),
      [storageCheckbox_ state] == NSOnState);
}

- (void)onCancel:(id)sender {
  bridge_->PerformClose();
}

- (void)onExpirationDateChanged:(id)sender {
  [self updateVerifyButtonEnabled];
}

// Called when text in CVC input field changes.
- (void)controlTextDidChange:(NSNotification*)notification {
  [self updateVerifyButtonEnabled];
}

- (base::scoped_nsobject<NSView>)createStorageViewWithController:
        (autofill::CardUnmaskPromptController*)controller {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);

  // "Store card on this device" checkbox.
  storageCheckbox_.reset([[NSButton alloc] initWithFrame:NSZeroRect]);
  [storageCheckbox_ setButtonType:NSSwitchButton];
  [storageCheckbox_
      setTitle:base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_CARD_UNMASK_PROMPT_STORAGE_CHECKBOX))];
  [storageCheckbox_
      setState:(controller->GetStoreLocallyStartState() ? NSOnState
                                                        : NSOffState)];
  [storageCheckbox_ sizeToFit];
  [view addSubview:storageCheckbox_];

  // "?" icon with tooltip.
  storageTooltip_.reset([[AutofillTooltipController alloc]
      initWithArrowLocation:info_bubble::kTopRight]);
  [storageTooltip_ setImage:ui::ResourceBundle::GetSharedInstance()
                                .GetNativeImageNamed(IDR_AUTOFILL_TOOLTIP_ICON)
                                .ToNSImage()];
  [storageTooltip_
      setMessage:base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_CARD_UNMASK_PROMPT_STORAGE_TOOLTIP))];
  [view addSubview:[storageTooltip_ view]];
  [[storageTooltip_ view]
      setFrameOrigin:NSMakePoint(NSMaxX([storageCheckbox_ frame]) + kButtonGap,
                                 0)];

  [CardUnmaskPromptViewCocoa sizeToFitView:view];
  [CardUnmaskPromptViewCocoa verticallyCenterSubviewsInView:view];
  return view;
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

  base::scoped_nsobject<NSView> storageView(
      [self createStorageViewWithController:controller]);
  [mainView addSubview:storageView];

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
    monthPopupDefaultIndex_ = monthModel.GetDefaultIndex();
    monthPopup_.reset(
        [CardUnmaskPromptViewCocoa buildDatePopupWithModel:monthModel]);
    [monthPopup_ setTarget:self];
    [monthPopup_ setAction:@selector(onExpirationDateChanged:)];
    [expirationView addSubview:monthPopup_];

    // Year.
    autofill::YearComboboxModel yearModel;
    yearPopupDefaultIndex_ = yearModel.GetDefaultIndex();
    yearPopup_.reset(
        [CardUnmaskPromptViewCocoa buildDatePopupWithModel:yearModel]);
    [yearPopup_ setTarget:self];
    [yearPopup_ setAction:@selector(onExpirationDateChanged:)];
    [expirationView addSubview:yearPopup_];

    // Layout month and year within expirationView.
    [yearPopup_
        setFrameOrigin:NSMakePoint(NSMaxX([monthPopup_ frame]) + kButtonGap,
                                   0)];
    NSRect expirationFrame =
        NSUnionRect([monthPopup_ frame], [yearPopup_ frame]);
    expirationFrame.size.width += kButtonGap;
    [expirationView setFrame:expirationFrame];
    [inputRowView addSubview:expirationView];
  }

  // CVC text input.
  cvcInput_.reset([[NSTextField alloc] initWithFrame:NSZeroRect]);
  [[cvcInput_ cell]
      setPlaceholderString:l10n_util::GetNSString(
                               IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC)];
  [[cvcInput_ cell] setScrollable:YES];
  [cvcInput_ setDelegate:self];
  [cvcInput_ sizeToFit];
  [cvcInput_ setFrame:NSMakeRect(NSMaxX([expirationView frame]), 0,
                                 kCvcInputWidth, NSHeight([cvcInput_ frame]))];
  [inputRowView addSubview:cvcInput_];

  // CVC image.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NSImage* cvcImage =
      rb.GetNativeImageNamed(controller->GetCvcImageRid()).ToNSImage();
  base::scoped_nsobject<NSImageView> cvcImageView(
      [[NSImageView alloc] initWithFrame:NSZeroRect]);
  [cvcImageView setImage:cvcImage];
  [cvcImageView setFrameSize:[cvcImage size]];
  [cvcImageView
      setFrameOrigin:NSMakePoint(NSMaxX([cvcInput_ frame]) + kButtonGap, 0)];
  [inputRowView addSubview:cvcImageView];

  // Cancel button.
  base::scoped_nsobject<NSButton> cancelButton(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [cancelButton setTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)];
  [cancelButton setKeyEquivalent:kKeyEquivalentEscape];
  [cancelButton setTarget:self];
  [cancelButton setAction:@selector(onCancel:)];
  [cancelButton sizeToFit];
  [mainView addSubview:cancelButton];

  // Verify button.
  verifyButton_.reset(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  // TODO(bondd): use l10n string.
  [verifyButton_ setTitle:@"Verify"];
  [verifyButton_ setKeyEquivalent:kKeyEquivalentReturn];
  [verifyButton_ setTarget:self];
  [verifyButton_ setAction:@selector(onVerify:)];
  [verifyButton_ sizeToFit];
  [self updateVerifyButtonEnabled];
  [mainView addSubview:verifyButton_];

  // Layout inputRowView.
  [CardUnmaskPromptViewCocoa sizeToFitView:inputRowView];
  [CardUnmaskPromptViewCocoa verticallyCenterSubviewsInView:inputRowView];

  // Calculate dialog content width.
  CGFloat contentWidth =
      std::max(NSWidth([title frame]), NSWidth([inputRowView frame]));
  contentWidth = std::max(contentWidth, NSWidth([storageView frame]));
  contentWidth = std::max(contentWidth, kDialogContentMinWidth);

  // Layout mainView contents, starting at the bottom and moving up.

  [storageView
      setFrameOrigin:NSMakePoint(0, chrome_style::kClientBottomPadding)];

  // Verify and Cancel buttons.
  [verifyButton_
      setFrameOrigin:NSMakePoint(contentWidth - NSWidth([verifyButton_ frame]),
                                 NSMaxY([storageView frame]) +
                                     chrome_style::kRowPadding)];

  [cancelButton
      setFrameOrigin:NSMakePoint(NSMinX([verifyButton_ frame]) - kButtonGap -
                                     NSWidth([cancelButton frame]),
                                 NSMinY([verifyButton_ frame]))];

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
