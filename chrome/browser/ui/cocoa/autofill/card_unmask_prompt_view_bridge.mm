// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/autofill/card_unmask_prompt_view_bridge.h"

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_pop_up_button.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_tooltip_controller.h"
#include "chrome/browser/ui/cocoa/chrome_style.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_control_utils.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#import "chrome/browser/ui/cocoa/spinner_view.h"
#include "chrome/browser/ui/cocoa/themed_window.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_controller.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/native_theme/native_theme.h"

namespace {

const CGFloat kButtonGap = 6.0f;
const CGFloat kButtonsToRetriableErrorGap = 12.0f;
const CGFloat kCvcInputWidth = 64.0f;
const CGFloat kCvcInputToImageGap = 8.0f;
const CGFloat kDialogContentMinWidth = 210.0f;
const CGFloat kInputRowToInstructionsGap = 16.0f;
const CGFloat kInstructionsToTitleGap = 8.0f;
const CGFloat kPermanentErrorExteriorPadding = 12.0f;
const CGFloat kPermanentErrorHorizontalPadding = 16.0f;
const CGFloat kPermanentErrorVerticalPadding = 12.0f;
const CGFloat kProgressToInstructionsGap = 24.0f;
const CGFloat kRetriableErrorToInputRowGap = 4.0f;
const CGFloat kSeparatorHeight = 1.0f;
const CGFloat kSpinnerSize = 16.0f;
const CGFloat kSpinnerToProgressTextGap = 8.0f;
const CGFloat kYearToCvcGap = 12.0f;

const SkColor kPermanentErrorTextColor = SK_ColorWHITE;
// TODO(bondd): Unify colors with Views version and AutofillMessageView.
const SkColor kShadingColor = SkColorSetRGB(0xf2, 0xf2, 0xf2);
const SkColor kSubtleBorderColor = SkColorSetRGB(0xdf, 0xdf, 0xdf);

}  // namespace

namespace autofill {

#pragma mark CardUnmaskPromptViewBridge

CardUnmaskPromptViewBridge::CardUnmaskPromptViewBridge(
    CardUnmaskPromptController* controller,
    content::WebContents* web_contents)
    : controller_(controller),
      web_contents_(web_contents),
      weak_ptr_factory_(this) {
  view_controller_.reset(
      [[CardUnmaskPromptViewCocoa alloc] initWithBridge:this]);
}

CardUnmaskPromptViewBridge::~CardUnmaskPromptViewBridge() {
}

void CardUnmaskPromptViewBridge::Show() {
  // Setup the constrained window that will show the view.
  base::scoped_nsobject<NSWindow> window([[ConstrainedWindowCustomWindow alloc]
      initWithContentRect:[[view_controller_ view] bounds]]);
  [window setContentView:[view_controller_ view]];
  base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc] initWithCustomWindow:window]);
  constrained_window_ =
      CreateAndShowWebModalDialogMac(this, web_contents_, sheet);
}

void CardUnmaskPromptViewBridge::ControllerGone() {
  controller_ = nullptr;
  PerformClose();
}

void CardUnmaskPromptViewBridge::DisableAndWaitForVerification() {
  [view_controller_ setProgressOverlayText:
                        l10n_util::GetStringUTF16(
                            IDS_AUTOFILL_CARD_UNMASK_VERIFICATION_IN_PROGRESS)
                               showSpinner:YES];
}

void CardUnmaskPromptViewBridge::GotVerificationResult(
    const base::string16& error_message,
    bool allow_retry) {
  if (error_message.empty()) {
    [view_controller_ setProgressOverlayText:
                          l10n_util::GetStringUTF16(
                              IDS_AUTOFILL_CARD_UNMASK_VERIFICATION_SUCCESS)
                                 showSpinner:NO];

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&CardUnmaskPromptViewBridge::PerformClose,
                              weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(1));
  } else {
    [view_controller_ setProgressOverlayText:base::string16() showSpinner:NO];

    if (allow_retry) {
      // TODO(bondd): Views version never hides |errorLabel_|. When Views
      // decides when to hide it then do the same thing here.
      [view_controller_ setRetriableErrorMessage:error_message];
    } else {
      [view_controller_ setPermanentErrorMessage:error_message];
    }
  }
}

void CardUnmaskPromptViewBridge::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  if (controller_)
    controller_->OnUnmaskDialogClosed();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

CardUnmaskPromptController* CardUnmaskPromptViewBridge::GetController() {
  return controller_;
}

content::WebContents* CardUnmaskPromptViewBridge::GetWebContents() {
  return web_contents_;
}

void CardUnmaskPromptViewBridge::PerformClose() {
  constrained_window_->CloseWebContentsModalDialog();
}

}  // autofill

#pragma mark CardUnmaskPromptViewCocoa

@implementation CardUnmaskPromptViewCocoa {
  base::scoped_nsobject<NSBox> permanentErrorBox_;
  base::scoped_nsobject<NSView> expirationView_;
  base::scoped_nsobject<NSView> inputRowView_;
  base::scoped_nsobject<NSView> progressOverlayView_;
  base::scoped_nsobject<NSView> storageView_;

  base::scoped_nsobject<NSTextField> titleLabel_;
  base::scoped_nsobject<NSTextField> permanentErrorLabel_;
  base::scoped_nsobject<NSTextField> instructionsLabel_;
  base::scoped_nsobject<AutofillTextField> cvcInput_;
  base::scoped_nsobject<AutofillPopUpButton> monthPopup_;
  base::scoped_nsobject<AutofillPopUpButton> yearPopup_;
  base::scoped_nsobject<NSImageView> cvcImageView_;
  base::scoped_nsobject<NSButton> newCardButton_;
  base::scoped_nsobject<NSButton> cancelButton_;
  base::scoped_nsobject<NSButton> verifyButton_;
  base::scoped_nsobject<NSButton> storageCheckbox_;
  base::scoped_nsobject<AutofillTooltipController> storageTooltip_;
  base::scoped_nsobject<NSTextField> errorLabel_;
  base::scoped_nsobject<NSTextField> progressOverlayLabel_;
  base::scoped_nsobject<SpinnerView> progressOverlaySpinner_;

  int monthPopupDefaultIndex_;
  int yearPopupDefaultIndex_;

  // Owns |self|.
  autofill::CardUnmaskPromptViewBridge* bridge_;
}

+ (AutofillPopUpButton*)buildDatePopupWithModel:(ui::ComboboxModel&)model {
  AutofillPopUpButton* popup =
      [[AutofillPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];

  for (int i = 0; i < model.GetItemCount(); ++i) {
    [popup addItemWithTitle:base::SysUTF16ToNSString(model.GetItemAt(i))];
  }
  [popup sizeToFit];
  return popup;
}

+ (base::scoped_nsobject<NSBox>)createPlainBox {
  base::scoped_nsobject<NSBox> box([[NSBox alloc] initWithFrame:NSZeroRect]);
  [box setBoxType:NSBoxCustom];
  [box setBorderType:NSNoBorder];
  [box setTitlePosition:NSNoTitle];
  return box;
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

- (void)updateProgressOverlayOrigin {
  // Center |progressOverlayView_| horizontally in the dialog, and position it
  // a fixed distance below |instructionsLabel_|.
  CGFloat viewMinY = NSMinY([instructionsLabel_ frame]) -
                     kProgressToInstructionsGap -
                     NSHeight([progressOverlayView_ frame]);
  [progressOverlayView_
      setFrameOrigin:NSMakePoint(
                         NSMidX([[self view] frame]) -
                             NSWidth([progressOverlayView_ frame]) / 2.0,
                         viewMinY)];
}

- (void)setProgressOverlayText:(const base::string16&)text
                   showSpinner:(BOOL)showSpinner {
  if (!text.empty()) {
    NSAttributedString* attributedString =
        constrained_window::GetAttributedLabelString(
            SysUTF16ToNSString(text), chrome_style::kTextFontStyle,
            NSNaturalTextAlignment, NSLineBreakByWordWrapping);
    [progressOverlayLabel_ setAttributedStringValue:attributedString];
    [progressOverlayLabel_ sizeToFit];
    CGFloat labelMinX = showSpinner
                            ? NSMaxX([progressOverlaySpinner_ frame]) +
                                  kSpinnerToProgressTextGap
                            : 0;
    [progressOverlayLabel_ setFrameOrigin:NSMakePoint(labelMinX, 0)];

    [CardUnmaskPromptViewCocoa sizeToFitView:progressOverlayView_];
    [self updateProgressOverlayOrigin];
  }

  [progressOverlayView_ setHidden:text.empty()];
  [progressOverlaySpinner_ setHidden:!showSpinner];
  [inputRowView_ setHidden:!text.empty()];
  [errorLabel_ setHidden:!text.empty()];
  [storageCheckbox_ setHidden:!text.empty()];
  [[storageTooltip_ view] setHidden:!text.empty()];
  [self updateVerifyButtonEnabled];
}

- (void)setInputsEnabled:(BOOL)enabled {
  [cvcInput_ setEnabled:enabled];
  [monthPopup_ setEnabled:enabled];
  [yearPopup_ setEnabled:enabled];
  [newCardButton_ setEnabled:enabled];
  [storageCheckbox_ setEnabled:enabled];
}

- (void)setRetriableErrorMessage:(const base::string16&)text {
  NSAttributedString* attributedString =
      constrained_window::GetAttributedLabelString(
          SysUTF16ToNSString(text), chrome_style::kTextFontStyle,
          NSNaturalTextAlignment, NSLineBreakByWordWrapping);
  [errorLabel_ setAttributedStringValue:attributedString];

  // If there is more than one input showing, don't mark anything as invalid
  // since we don't know the location of the problem.
  if (!text.empty() &&
      !bridge_->GetController()->ShouldRequestExpirationDate()) {
    [cvcInput_ setValidityMessage:@"invalid"];

    // Show "New card?" button, which when clicked will cause this dialog to
    // ask for expiration date.
    [self createNewCardButton];
    [inputRowView_ addSubview:newCardButton_];
  }

  [self performLayoutAndDisplay:YES];
}

- (void)setPermanentErrorMessage:(const base::string16&)text {
  if (!text.empty()) {
    if (!permanentErrorBox_) {
      permanentErrorBox_ = [CardUnmaskPromptViewCocoa createPlainBox];
      [permanentErrorBox_
          setFillColor:skia::SkColorToCalibratedNSColor(gfx::kGoogleRed700)];
      [permanentErrorBox_
          setContentViewMargins:NSMakeSize(kPermanentErrorHorizontalPadding,
                                           kPermanentErrorVerticalPadding)];

      permanentErrorLabel_.reset([constrained_window::CreateLabel() retain]);
      [permanentErrorLabel_ setAutoresizingMask:NSViewWidthSizable];
      [permanentErrorLabel_ setTextColor:skia::SkColorToCalibratedNSColor(
                                             kPermanentErrorTextColor)];

      [permanentErrorBox_ addSubview:permanentErrorLabel_];
      [[self view] addSubview:permanentErrorBox_];
    }

    NSAttributedString* attributedString =
        constrained_window::GetAttributedLabelString(
            SysUTF16ToNSString(text), chrome_style::kTextFontStyle,
            NSNaturalTextAlignment, NSLineBreakByWordWrapping);
    [permanentErrorLabel_ setAttributedStringValue:attributedString];
  }

  [permanentErrorBox_ setHidden:text.empty()];
  [self setInputsEnabled:NO];
  [self updateVerifyButtonEnabled];
  [self setRetriableErrorMessage:base::string16()];
}

- (void)updateVerifyButtonEnabled {
  BOOL enable = ![inputRowView_ isHidden] &&
                ![[permanentErrorLabel_ stringValue] length] &&
                bridge_->GetController()->InputCvcIsValid(
                    base::SysNSStringToUTF16([cvcInput_ stringValue])) &&
                [self expirationDateIsValid];

  [verifyButton_ setEnabled:enable];
}

- (void)onVerify:(id)sender {
  bridge_->GetController()->OnUnmaskResponse(
      base::SysNSStringToUTF16([cvcInput_ stringValue]),
      base::SysNSStringToUTF16([monthPopup_ titleOfSelectedItem]),
      base::SysNSStringToUTF16([yearPopup_ titleOfSelectedItem]),
      [storageCheckbox_ state] == NSOnState);
}

- (void)onCancel:(id)sender {
  bridge_->PerformClose();
}

- (void)onNewCard:(id)sender {
  autofill::CardUnmaskPromptController* controller = bridge_->GetController();
  controller->NewCardLinkClicked();

  // |newCardButton_| will never be shown again for this dialog.
  [newCardButton_ removeFromSuperview];
  newCardButton_.reset();

  [self createExpirationView];
  [inputRowView_ addSubview:expirationView_];
  [cvcInput_ setStringValue:@""];
  [cvcInput_ setValidityMessage:@""];
  [self setRetriableErrorMessage:base::string16()];
  [self updateInstructionsText];
  [self updateVerifyButtonEnabled];
  [self performLayoutAndDisplay:YES];
}

- (BOOL)expirationDateIsValid {
  if (!bridge_->GetController()->ShouldRequestExpirationDate())
    return true;

  return bridge_->GetController()->InputExpirationIsValid(
      base::SysNSStringToUTF16([monthPopup_ titleOfSelectedItem]),
      base::SysNSStringToUTF16([yearPopup_ titleOfSelectedItem]));
}

- (void)onExpirationDateChanged:(id)sender {
  if ([self expirationDateIsValid]) {
    if ([monthPopup_ invalid]) {
      [monthPopup_ setValidityMessage:@""];
      [yearPopup_ setValidityMessage:@""];
      [self setRetriableErrorMessage:base::string16()];
    }
  } else if ([monthPopup_ indexOfSelectedItem] != monthPopupDefaultIndex_ &&
             [yearPopup_ indexOfSelectedItem] != yearPopupDefaultIndex_) {
    [monthPopup_ setValidityMessage:@"invalid"];
    [yearPopup_ setValidityMessage:@"invalid"];
    [self setRetriableErrorMessage:
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_CARD_UNMASK_INVALID_EXPIRATION_DATE)];
  }

  [self updateVerifyButtonEnabled];
}

// Called when text in CVC input field changes.
- (void)controlTextDidChange:(NSNotification*)notification {
  if (bridge_->GetController()->InputCvcIsValid(
          base::SysNSStringToUTF16([cvcInput_ stringValue])))
    [cvcInput_ setValidityMessage:@""];

  [self updateVerifyButtonEnabled];
}

- (void)updateInstructionsText {
  NSAttributedString* instructionsString =
      constrained_window::GetAttributedLabelString(
          SysUTF16ToNSString(
              bridge_->GetController()->GetInstructionsMessage()),
          chrome_style::kTextFontStyle, NSNaturalTextAlignment,
          NSLineBreakByWordWrapping);
  [instructionsLabel_ setAttributedStringValue:instructionsString];
}

- (void)createNewCardButton {
  DCHECK(!newCardButton_);
  newCardButton_.reset([[HyperlinkButtonCell
      buttonWithString:l10n_util::GetNSString(
                           IDS_AUTOFILL_CARD_UNMASK_NEW_CARD_LINK)] retain]);
  [newCardButton_ setTarget:self];
  [newCardButton_ setAction:@selector(onNewCard:)];
  [newCardButton_ sizeToFit];
}

- (void)createExpirationView {
  DCHECK(!expirationView_);
  expirationView_.reset([[NSView alloc] initWithFrame:NSZeroRect]);

  // Add expiration month.
  autofill::MonthComboboxModel monthModel;
  monthPopupDefaultIndex_ = monthModel.GetDefaultIndex();
  monthPopup_.reset(
      [CardUnmaskPromptViewCocoa buildDatePopupWithModel:monthModel]);
  [monthPopup_ setTarget:self];
  [monthPopup_ setAction:@selector(onExpirationDateChanged:)];
  [expirationView_ addSubview:monthPopup_];

  // Add separator between month and year.
  base::scoped_nsobject<NSTextField> separatorLabel(
      [constrained_window::CreateLabel() retain]);
  NSAttributedString* separatorString =
      constrained_window::GetAttributedLabelString(
          SysUTF16ToNSString(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_CARD_UNMASK_EXPIRATION_DATE_SEPARATOR)),
          chrome_style::kTextFontStyle, NSNaturalTextAlignment,
          NSLineBreakByWordWrapping);
  [separatorLabel setAttributedStringValue:separatorString];
  [separatorLabel sizeToFit];
  [expirationView_ addSubview:separatorLabel];

  // Add expiration year.
  autofill::YearComboboxModel yearModel;
  yearPopupDefaultIndex_ = yearModel.GetDefaultIndex();
  yearPopup_.reset(
      [CardUnmaskPromptViewCocoa buildDatePopupWithModel:yearModel]);
  [yearPopup_ setTarget:self];
  [yearPopup_ setAction:@selector(onExpirationDateChanged:)];
  [expirationView_ addSubview:yearPopup_];

  // Lay out month, separator, and year within |expirationView_|.
  [separatorLabel setFrameOrigin:NSMakePoint(NSMaxX([monthPopup_ frame]), 0)];
  [yearPopup_ setFrameOrigin:NSMakePoint(NSMaxX([separatorLabel frame]), 0)];
  NSRect expirationFrame = NSUnionRect([monthPopup_ frame], [yearPopup_ frame]);
  expirationFrame.size.width += kYearToCvcGap;
  [expirationView_ setFrame:expirationFrame];
  [CardUnmaskPromptViewCocoa verticallyCenterSubviewsInView:expirationView_];
}

- (void)createStorageViewWithController:
        (autofill::CardUnmaskPromptController*)controller {
  DCHECK(!storageView_);
  storageView_.reset([[NSView alloc] initWithFrame:NSZeroRect]);
  [storageView_ setAutoresizingMask:NSViewWidthSizable];

  base::scoped_nsobject<NSBox> box([CardUnmaskPromptViewCocoa createPlainBox]);
  [box setAutoresizingMask:NSViewWidthSizable];
  [box setFillColor:skia::SkColorToCalibratedNSColor(kShadingColor)];
  [box setContentViewMargins:NSMakeSize(chrome_style::kHorizontalPadding,
                                        chrome_style::kClientBottomPadding)];
  [storageView_ addSubview:box];

  // Add "Store card on this device" checkbox.
  storageCheckbox_.reset([[NSButton alloc] initWithFrame:NSZeroRect]);
  [storageCheckbox_ setButtonType:NSSwitchButton];
  [storageCheckbox_
      setTitle:base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_CARD_UNMASK_PROMPT_STORAGE_CHECKBOX))];
  [storageCheckbox_
      setState:(controller->GetStoreLocallyStartState() ? NSOnState
                                                        : NSOffState)];
  [storageCheckbox_ sizeToFit];
  [box addSubview:storageCheckbox_];

  // Add "i" icon with tooltip.
  storageTooltip_.reset([[AutofillTooltipController alloc]
      initWithArrowLocation:info_bubble::kTopTrailing]);
  [storageTooltip_ setMaxTooltipWidth:2 * autofill::kFieldWidth +
                                      autofill::kHorizontalFieldPadding];
  [storageTooltip_
      setImage:gfx::NSImageFromImageSkia(gfx::CreateVectorIcon(
                   gfx::VectorIconId::INFO_OUTLINE, autofill::kInfoIconSize,
                   gfx::kChromeIconGrey))];
  [storageTooltip_
      setMessage:base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_CARD_UNMASK_PROMPT_STORAGE_TOOLTIP))];
  [box addSubview:[storageTooltip_ view]];
  [[storageTooltip_ view] setFrameOrigin:
      NSMakePoint(NSMaxX([storageCheckbox_ frame]) + kButtonGap, 0)];

  // Add horizontal separator.
  base::scoped_nsobject<NSBox> separator(
      [CardUnmaskPromptViewCocoa createPlainBox]);
  [separator setAutoresizingMask:NSViewWidthSizable];
  [separator setFillColor:skia::SkColorToCalibratedNSColor(kSubtleBorderColor)];
  [storageView_ addSubview:separator];

  [box sizeToFit];
  [separator setFrame:NSMakeRect(0, NSMaxY([box frame]), NSWidth([box frame]),
                                 kSeparatorHeight)];
  [CardUnmaskPromptViewCocoa sizeToFitView:storageView_];
}

// +---------------------------------------------------------------------------+
// | titleLabel_        (Single line.)                                         |
// |---------------------------------------------------------------------------|
// | permanentErrorBox_ (Multiline, may be hidden.)                            |
// |---------------------------------------------------------------------------|
// | instructionsLabel_ (Multiline.)                                           |
// |---------------------------------------------------------------------------|
// | monthPopup_ yearPopup_ cvcInput_ cvcImageView_ newCardButton_             |
// |     (All enclosed in inputRowView_. month, year, and newCard may be       |
// |      hidden.)                                                             |
// |---------------------------------------------------------------------------|
// | errorLabel_ (Multiline. Always takes up space for one line even if empty. |
// |              Hidden when progressOverlayView_ is displayed.)              |
// |---------------------------------------------------------------------------|
// |                                                         [Cancel] [Verify] |
// |---------------------------------------------------------------------------|
// | separator (NSBox.)                                                        |
// | storageCheckbox_ storageTooltip_ (Both enclosed in another NSBox.         |
// |                                   Checkbox and tooltip may be hidden.)    |
// |     (Both NSBoxes are enclosed in storageView_. Will all be nil if        |
// |      !CanStoreLocally()).                                                 |
// +---------------------------------------------------------------------------+
//
// progressOverlayView_:
//     (Displayed below instructionsLabel_, may be hidden.
//      progressOverlaySpinner_ may be hidden while progressOverlayLabel_
//      is shown.)
// +---------------------------------------------------------------------------+
// | progressOverlaySpinner_ progressOverlayLabel_                             |
// +---------------------------------------------------------------------------+
- (void)performLayoutAndDisplay:(BOOL)display {
  // Lay out |inputRowView_| contents.
  // |expirationView_| may be nil, which will result in |cvcInput_| getting an
  // x value of 0.
  [cvcInput_ setFrame:NSMakeRect(NSMaxX([expirationView_ frame]), 0,
                                 kCvcInputWidth, NSHeight([cvcInput_ frame]))];
  [cvcImageView_
      setFrameOrigin:NSMakePoint(
                         NSMaxX([cvcInput_ frame]) + kCvcInputToImageGap, 0)];
  [newCardButton_
      setFrameOrigin:NSMakePoint(
                         NSMaxX([cvcImageView_ frame]) + kButtonGap, 0)];
  [CardUnmaskPromptViewCocoa sizeToFitView:inputRowView_];
  [CardUnmaskPromptViewCocoa verticallyCenterSubviewsInView:inputRowView_];

  // Calculate dialog content width.
  CGFloat contentWidth =
      std::max(NSWidth([titleLabel_ frame]), NSWidth([inputRowView_ frame]));
  contentWidth = std::max(contentWidth,
                          NSWidth(NSUnionRect([storageCheckbox_ frame],
                                              [[storageTooltip_ view] frame])));
  contentWidth = std::max(contentWidth, kDialogContentMinWidth);

  CGFloat contentMinX = chrome_style::kHorizontalPadding;
  CGFloat contentMaxX = contentMinX + contentWidth;
  CGFloat dialogWidth = contentMaxX + chrome_style::kHorizontalPadding;

  CGFloat verifyMinY =
      storageView_ ? NSMaxY([storageView_ frame]) + chrome_style::kRowPadding
                   : chrome_style::kClientBottomPadding;
  [verifyButton_
      setFrameOrigin:NSMakePoint(contentMaxX - NSWidth([verifyButton_ frame]),
                                 verifyMinY)];

  [cancelButton_
      setFrameOrigin:NSMakePoint(NSMinX([verifyButton_ frame]) - kButtonGap -
                                     NSWidth([cancelButton_ frame]),
                                 NSMinY([verifyButton_ frame]))];

  [errorLabel_ setFrame:NSMakeRect(contentMinX, NSMaxY([cancelButton_ frame]) +
                                                    kButtonsToRetriableErrorGap,
                                   contentWidth, 0)];
  cocoa_l10n_util::WrapOrSizeToFit(errorLabel_);

  [inputRowView_ setFrameOrigin:NSMakePoint(contentMinX,
                                            NSMaxY([errorLabel_ frame]) +
                                                kRetriableErrorToInputRowGap)];

  [instructionsLabel_
      setFrame:NSMakeRect(contentMinX, NSMaxY([inputRowView_ frame]) +
                                           kInputRowToInstructionsGap,
                          contentWidth, 0)];
  cocoa_l10n_util::WrapOrSizeToFit(instructionsLabel_);

  // Lay out permanent error box.
  CGFloat titleMinY;
  if (permanentErrorBox_ && ![permanentErrorBox_ isHidden]) {
    [permanentErrorBox_
        setFrame:NSMakeRect(0, NSMaxY([instructionsLabel_ frame]) +
                                   kPermanentErrorExteriorPadding,
                            dialogWidth, 0)];
    cocoa_l10n_util::WrapOrSizeToFit(permanentErrorLabel_);
    [permanentErrorBox_ sizeToFit];
    titleMinY =
        NSMaxY([permanentErrorBox_ frame]) + kPermanentErrorExteriorPadding;
  } else {
    titleMinY = NSMaxY([instructionsLabel_ frame]) + kInstructionsToTitleGap;
  }

  [titleLabel_ setFrameOrigin:NSMakePoint(contentMinX, titleMinY)];

  // Set dialog size.
  [[self view]
      setFrameSize:NSMakeSize(dialogWidth, NSMaxY([titleLabel_ frame]) +
                                               chrome_style::kTitleTopPadding)];

  [self updateProgressOverlayOrigin];

  NSRect frameRect =
      [[[self view] window] frameRectForContentRect:[[self view] frame]];
  [[[self view] window] setFrame:frameRect display:display];
}

- (void)loadView {
  autofill::CardUnmaskPromptController* controller = bridge_->GetController();
  DCHECK(controller);

  base::scoped_nsobject<NSView> mainView(
      [[NSView alloc] initWithFrame:NSZeroRect]);

  inputRowView_.reset([[NSView alloc] initWithFrame:NSZeroRect]);
  [mainView addSubview:inputRowView_];

  if (controller->CanStoreLocally()) {
    [self createStorageViewWithController:controller];
    [mainView addSubview:storageView_];
  }

  // Add progress overlay.
  progressOverlayView_.reset([[NSView alloc] initWithFrame:NSZeroRect]);
  [progressOverlayView_ setHidden:YES];
  [mainView addSubview:progressOverlayView_];

  progressOverlayLabel_.reset([constrained_window::CreateLabel() retain]);
  NSColor* throbberBlueColor = skia::SkColorToCalibratedNSColor(
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_ThrobberSpinningColor));
  [progressOverlayLabel_ setTextColor:throbberBlueColor];
  [progressOverlayView_ addSubview:progressOverlayLabel_];

  progressOverlaySpinner_.reset([[SpinnerView alloc]
      initWithFrame:NSMakeRect(0, 0, kSpinnerSize, kSpinnerSize)]);
  [progressOverlayView_ addSubview:progressOverlaySpinner_];

  // Add title label.
  titleLabel_.reset([constrained_window::CreateLabel() retain]);
  NSAttributedString* titleString =
      constrained_window::GetAttributedLabelString(
          SysUTF16ToNSString(controller->GetWindowTitle()),
          chrome_style::kTitleFontStyle, NSNaturalTextAlignment,
          NSLineBreakByWordWrapping);
  [titleLabel_ setAttributedStringValue:titleString];
  [titleLabel_ sizeToFit];
  [mainView addSubview:titleLabel_];

  // Add instructions label.
  instructionsLabel_.reset([constrained_window::CreateLabel() retain]);
  [self updateInstructionsText];
  [mainView addSubview:instructionsLabel_];

  // Add expiration date.
  if (controller->ShouldRequestExpirationDate()) {
    [self createExpirationView];
    [inputRowView_ addSubview:expirationView_];
  }

  // Add CVC text input.
  cvcInput_.reset([[AutofillTextField alloc] initWithFrame:NSZeroRect]);
  [[cvcInput_ cell]
      setPlaceholderString:l10n_util::GetNSString(
                               IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC)];
  [[cvcInput_ cell] setScrollable:YES];
  [cvcInput_ setDelegate:self];
  [cvcInput_ sizeToFit];
  [inputRowView_ addSubview:cvcInput_];

  // Add CVC image.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NSImage* cvcImage =
      rb.GetNativeImageNamed(controller->GetCvcImageRid()).ToNSImage();
  cvcImageView_.reset([[NSImageView alloc] initWithFrame:NSZeroRect]);
  [cvcImageView_ setImage:cvcImage];
  [cvcImageView_ setFrameSize:[cvcImage size]];
  [inputRowView_ addSubview:cvcImageView_];

  // Add error message label.
  errorLabel_.reset([constrained_window::CreateLabel() retain]);
  [errorLabel_
      setTextColor:skia::SkColorToCalibratedNSColor(gfx::kGoogleRed700)];
  [mainView addSubview:errorLabel_];

  // Add cancel button.
  cancelButton_.reset(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [cancelButton_ setTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)];
  [cancelButton_ setKeyEquivalent:kKeyEquivalentEscape];
  [cancelButton_ setTarget:self];
  [cancelButton_ setAction:@selector(onCancel:)];
  [cancelButton_ sizeToFit];
  [mainView addSubview:cancelButton_];

  // Add verify button.
  verifyButton_.reset(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [verifyButton_ setTitle:l10n_util::FixUpWindowsStyleLabel(
      controller->GetOkButtonLabel())];
  [verifyButton_ setKeyEquivalent:kKeyEquivalentReturn];
  [verifyButton_ setTarget:self];
  [verifyButton_ setAction:@selector(onVerify:)];
  [verifyButton_ sizeToFit];
  [self updateVerifyButtonEnabled];
  [mainView addSubview:verifyButton_];

  [self setView:mainView];
  [self performLayoutAndDisplay:NO];
}

@end
