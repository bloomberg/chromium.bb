// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"

#include "base/bind.h"
#include "base/ios/ios_util.h"
#include "base/location.h"
#include "base/mac/foundation_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_controller.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/autofill/cells/cvc_item.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/autofill/cells/storage_switch_item.h"
#import "ios/chrome/browser/ui/autofill/storage_switch_tooltip.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kCardUnmaskPromptCollectionViewAccessibilityID =
    @"kCardUnmaskPromptCollectionViewAccessibilityID";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierMain = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCVC = kItemTypeEnumZero,
  ItemTypeStatus,
  ItemTypeStorageSwitch,
};

}  // namespace

namespace autofill {

#pragma mark CardUnmaskPromptViewBridge

CardUnmaskPromptViewBridge::CardUnmaskPromptViewBridge(
    CardUnmaskPromptController* controller,
    UIViewController* base_view_controller)
    : controller_(controller),
      base_view_controller_(base_view_controller),
      weak_ptr_factory_(this) {
  DCHECK(controller_);
}

CardUnmaskPromptViewBridge::~CardUnmaskPromptViewBridge() {
  if (controller_)
    controller_->OnUnmaskDialogClosed();
}

void CardUnmaskPromptViewBridge::Show() {
  view_controller_ =
      [[CardUnmaskPromptViewController alloc] initWithBridge:this];
  [view_controller_ setModalPresentationStyle:UIModalPresentationFormSheet];
  [view_controller_
      setModalTransitionStyle:UIModalTransitionStyleCoverVertical];
  [base_view_controller_ presentViewController:view_controller_
                                      animated:YES
                                    completion:nil];
}

void CardUnmaskPromptViewBridge::ControllerGone() {
  controller_ = nullptr;
  PerformClose();
}

void CardUnmaskPromptViewBridge::DisableAndWaitForVerification() {
  [view_controller_ showSpinner];
}

void CardUnmaskPromptViewBridge::GotVerificationResult(
    const base::string16& error_message,
    bool allow_retry) {
  if (error_message.empty()) {
    [view_controller_ showSuccess];
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&CardUnmaskPromptViewBridge::PerformClose,
                              weak_ptr_factory_.GetWeakPtr()),
        controller_->GetSuccessMessageDuration());
  } else {
    if (allow_retry) {
      [view_controller_
          showCVCInputFormWithError:base::SysUTF16ToNSString(error_message)];
    } else {
      [view_controller_ showError:base::SysUTF16ToNSString(error_message)];
    }
  }
}

CardUnmaskPromptController* CardUnmaskPromptViewBridge::GetController() {
  return controller_;
}

void CardUnmaskPromptViewBridge::PerformClose() {
  [view_controller_ dismissViewControllerAnimated:YES
                                       completion:^{
                                         this->DeleteSelf();
                                       }];
}

void CardUnmaskPromptViewBridge::DeleteSelf() {
  delete this;
}

}  // autofill

@interface CardUnmaskPromptViewController ()<UITextFieldDelegate> {
  UIBarButtonItem* _cancelButton;
  UIBarButtonItem* _verifyButton;
  CVCItem* _CVCItem;
  StatusItem* _statusItem;
  StorageSwitchItem* _storageSwitchItem;

  // The tooltip is added as a child of the collection view rather than the
  // StorageSwitchContentView to allow it to overflow the bounds of the switch
  // view.
  StorageSwitchTooltip* _storageSwitchTooltip;

  // Owns |self|.
  autofill::CardUnmaskPromptViewBridge* _bridge;  // weak
}

@end

@implementation CardUnmaskPromptViewController

- (instancetype)initWithBridge:(autofill::CardUnmaskPromptViewBridge*)bridge {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  DCHECK(bridge);
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    _bridge = bridge;
    self.title =
        base::SysUTF16ToNSString(_bridge->GetController()->GetWindowTitle());
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.collectionView.accessibilityIdentifier =
      kCardUnmaskPromptCollectionViewAccessibilityID;

  self.styler.cellStyle = MDCCollectionViewCellStyleCard;

  [self showCVCInputForm];

  // Add the navigation buttons.
  _cancelButton =
      [[UIBarButtonItem alloc] initWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(onCancel:)];
  self.navigationItem.leftBarButtonItem = _cancelButton;

  NSString* verifyButtonText =
      base::SysUTF16ToNSString(_bridge->GetController()->GetOkButtonLabel());
  _verifyButton =
      [[UIBarButtonItem alloc] initWithTitle:verifyButtonText
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(onVerify:)];
  [_verifyButton setTitleTextAttributes:@{
    NSForegroundColorAttributeName : [[MDCPalette cr_bluePalette] tint600]
  }
                               forState:UIControlStateNormal];
  [_verifyButton setTitleTextAttributes:@{
    NSForegroundColorAttributeName : [UIColor lightGrayColor]
  }
                               forState:UIControlStateDisabled];
  [_verifyButton setEnabled:NO];
  self.navigationItem.rightBarButtonItem = _verifyButton;
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  if ([self.collectionViewModel hasItemForItemType:ItemTypeCVC
                                 sectionIdentifier:SectionIdentifierMain]) {
    NSIndexPath* CVCIndexPath =
        [self.collectionViewModel indexPathForItemType:ItemTypeCVC
                                     sectionIdentifier:SectionIdentifierMain];
    CVCCell* CVC = base::mac::ObjCCastStrict<CVCCell>(
        [self.collectionView cellForItemAtIndexPath:CVCIndexPath]);
    [self focusInputIfNeeded:CVC];
  }
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;
  [model addSectionWithIdentifier:SectionIdentifierMain];

  autofill::CardUnmaskPromptController* controller = _bridge->GetController();
  NSString* instructions =
      base::SysUTF16ToNSString(controller->GetInstructionsMessage());
  int CVCImageResourceID = controller->GetCvcImageRid();
  _CVCItem = [[CVCItem alloc] initWithType:ItemTypeCVC];
  _CVCItem.instructionsText = instructions;
  _CVCItem.CVCImageResourceID = CVCImageResourceID;
  [model addItem:_CVCItem toSectionWithIdentifier:SectionIdentifierMain];

  if (controller->CanStoreLocally()) {
    _storageSwitchItem =
        [[StorageSwitchItem alloc] initWithType:ItemTypeStorageSwitch];
    _storageSwitchItem.on = controller->GetStoreLocallyStartState();
    [model addItem:_storageSwitchItem
        toSectionWithIdentifier:SectionIdentifierMain];

    _storageSwitchTooltip = [[StorageSwitchTooltip alloc] init];
    [_storageSwitchTooltip setHidden:YES];
    [self.collectionView addSubview:_storageSwitchTooltip];
  } else {
    _storageSwitchItem = nil;
  }

  // No status item when loading the model.
  _statusItem = nil;
}

#pragma mark - Private

- (void)showCVCInputForm {
  [self showCVCInputFormWithError:nil];
}

- (void)showCVCInputFormWithError:(NSString*)errorMessage {
  [_verifyButton setEnabled:NO];

  [self loadModel];
  [self.collectionView reloadData];

  _CVCItem.errorMessage = errorMessage;
  // If the server requested a new expiration date, show the date input. If it
  // didn't and there was an error, show the "New card?" link which will show
  // the date inputs on click. This link is intended to remind the user that
  // they might have recently received a new card with updated expiration date
  // and CVC. At the same time, we only put the CVC input in an error state if
  // we're not requesting a new date. Because if we're asking the user for both,
  // we don't know which is incorrect.
  if (_bridge->GetController()->ShouldRequestExpirationDate()) {
    _CVCItem.showDateInput = YES;
  } else if (errorMessage) {
    _CVCItem.showNewCardButton = YES;
    _CVCItem.showCVCInputError = YES;
  }
}

- (void)showSpinner {
  [_verifyButton setEnabled:NO];
  [_storageSwitchTooltip setHidden:YES];

  [self
      updateWithStatus:StatusItemState::VERIFYING
                  text:l10n_util::GetNSString(
                           IDS_AUTOFILL_CARD_UNMASK_VERIFICATION_IN_PROGRESS)];
}

- (void)showSuccess {
  [_verifyButton setEnabled:NO];

  [self updateWithStatus:StatusItemState::VERIFIED
                    text:l10n_util::GetNSString(
                             IDS_AUTOFILL_CARD_UNMASK_VERIFICATION_SUCCESS)];
}

- (void)showError:(NSString*)errorMessage {
  [_cancelButton setTitle:l10n_util::GetNSString(IDS_CLOSE)];
  [_verifyButton setEnabled:NO];

  [self updateWithStatus:StatusItemState::ERROR text:errorMessage];
}

- (void)updateWithStatus:(StatusItemState)state text:(NSString*)text {
  if (!_statusItem) {
    _statusItem = [[StatusItem alloc] initWithType:ItemTypeStatus];
    _statusItem.text = text;
    _statusItem.state = state;
    // Remove all the present items to replace them with the status item.
    [self.collectionViewModel
        removeSectionWithIdentifier:SectionIdentifierMain];
    [self.collectionViewModel addSectionWithIdentifier:SectionIdentifierMain];
    [self.collectionViewModel addItem:_statusItem
              toSectionWithIdentifier:SectionIdentifierMain];
    [self.collectionView reloadData];
  } else {
    _statusItem.text = text;
    _statusItem.state = state;
    [self reconfigureCellsForItems:@[ _statusItem ]];
    [self.collectionViewLayout invalidateLayout];
  }
}

- (CGFloat)statusCellHeight {
  const CGFloat collectionViewWidth =
      CGRectGetWidth(self.collectionView.bounds);

  // The status cell replaces the previous content of the collection. So it is
  // sized based on what appears when not loading.
  const CGFloat preferredHeightForCVC =
      [MDCCollectionViewCell cr_preferredHeightForWidth:collectionViewWidth
                                                forItem:_CVCItem];
  CGFloat preferredHeightForStorageSwitch = 0;
  if (_storageSwitchItem) {
    preferredHeightForStorageSwitch =
        [MDCCollectionViewCell cr_preferredHeightForWidth:collectionViewWidth
                                                  forItem:_storageSwitchItem];
  }
  const CGFloat preferredHeightForStatus =
      [MDCCollectionViewCell cr_preferredHeightForWidth:collectionViewWidth
                                                forItem:_statusItem];
  // Return the size of the replaced content, but make sure it is at least the
  // minimal status cell height.
  return MAX(preferredHeightForCVC + preferredHeightForStorageSwitch,
             preferredHeightForStatus);
}

- (void)layoutTooltipFromButton:(UIButton*)button {
  const CGRect buttonFrameInCollectionView =
      [self.collectionView convertRect:button.bounds fromView:button];
  CGRect tooltipFrame = _storageSwitchTooltip.frame;

  // First, set the width and use sizeToFit to have the label flow the text and
  // set the height appropriately.
  const CGFloat kTooltipMargin = 8;
  CGFloat availableWidth =
      CGRectGetMinX(buttonFrameInCollectionView) - 2 * kTooltipMargin;
  const CGFloat kMaxTooltipWidth = 210;
  tooltipFrame.size.width = MIN(availableWidth, kMaxTooltipWidth);
  _storageSwitchTooltip.frame = tooltipFrame;
  [_storageSwitchTooltip sizeToFit];

  // Then use the size to position the tooltip appropriately, based on the
  // button position.
  tooltipFrame = _storageSwitchTooltip.frame;
  tooltipFrame.origin.x = CGRectGetMinX(buttonFrameInCollectionView) -
                          kTooltipMargin - CGRectGetWidth(tooltipFrame);
  tooltipFrame.origin.y = CGRectGetMaxY(buttonFrameInCollectionView) -
                          CGRectGetHeight(tooltipFrame);
  _storageSwitchTooltip.frame = tooltipFrame;
}

- (BOOL)inputCVCIsValid:(CVCItem*)item {
  return _bridge->GetController()->InputCvcIsValid(
      base::SysNSStringToUTF16(item.CVCText));
}

- (BOOL)inputExpirationIsValid:(CVCItem*)item {
  if (!item.showDateInput) {
    return YES;
  }

  return _bridge->GetController()->InputExpirationIsValid(
      base::SysNSStringToUTF16(item.monthText),
      base::SysNSStringToUTF16(item.yearText));
}

- (void)inputsDidChange:(CVCItem*)item {
  [_verifyButton setEnabled:[self inputCVCIsValid:item] &&
                            [self inputExpirationIsValid:item]];
}

- (void)updateDateErrorState:(CVCItem*)item {
  // Only change the error state if the inputs are of a length that can be
  // interpreted as valid or not.
  NSUInteger monthTextLength = item.monthText.length;
  if (monthTextLength != 1 && monthTextLength != 2) {
    return;
  }
  NSUInteger yearTextLength = item.yearText.length;
  if (yearTextLength != 2 && yearTextLength != 4) {
    return;
  }

  if ([self inputExpirationIsValid:item]) {
    item.showDateInputError = NO;
    item.errorMessage = @"";
  } else {
    item.showDateInputError = NO;
    item.errorMessage = l10n_util::GetNSString(
        IDS_AUTOFILL_CARD_UNMASK_INVALID_EXPIRATION_DATE);
  }

  [self reconfigureCellsForItems:@[ item ]];
  [self.collectionViewLayout invalidateLayout];
}

- (void)focusInputIfNeeded:(CVCCell*)CVC {
  // Focus the first visible input, unless the orientation is landscape. In
  // landscape, the keyboard covers up the storage checkbox shown below this
  // view and the user might never see it.
  if (UIInterfaceOrientationIsPortrait(
          [UIApplication sharedApplication].statusBarOrientation)) {
    // Also check whether any of the inputs are already the first responder and
    // are non-empty, in which case the focus should be left there.
    if ((!CVC.monthInput.isFirstResponder || CVC.monthInput.text.length == 0) &&
        (!CVC.yearInput.isFirstResponder || CVC.yearInput.text.length == 0) &&
        (!CVC.CVCInput.isFirstResponder || CVC.CVCInput.text.length == 0)) {
      if (_CVCItem.showDateInput) {
        [CVC.monthInput becomeFirstResponder];
      } else {
        [CVC.CVCInput becomeFirstResponder];
      }
    }
  }
}

#pragma mark - Actions

- (void)onVerify:(id)sender {
  autofill::CardUnmaskPromptController* controller = _bridge->GetController();
  DCHECK(controller);

  // The controller requires a 4-digit year. Convert if necessary.
  NSString* yearText = _CVCItem.yearText;
  if (yearText.length == 2) {
    NSInteger inputYear = yearText.integerValue;
    NSInteger currentYear =
        [[NSCalendar currentCalendar] components:NSCalendarUnitYear
                                        fromDate:[NSDate date]]
            .year;
    inputYear += currentYear - (currentYear % 100);
    yearText = [@(inputYear) stringValue];
  }

  controller->OnUnmaskResponse(base::SysNSStringToUTF16(_CVCItem.CVCText),
                               base::SysNSStringToUTF16(_CVCItem.monthText),
                               base::SysNSStringToUTF16(yearText),
                               _storageSwitchItem.on);
}

- (void)onCancel:(id)sender {
  _bridge->PerformClose();
}

- (void)onTooltipButtonTapped:(UIButton*)button {
  BOOL shouldShowTooltip = !button.selected;
  button.highlighted = shouldShowTooltip;
  if (shouldShowTooltip) {
    button.selected = YES;
    [self layoutTooltipFromButton:button];
    [_storageSwitchTooltip setHidden:NO];
  } else {
    button.selected = NO;
    [_storageSwitchTooltip setHidden:YES];
  }
}

- (void)onStorageSwitchChanged:(UISwitch*)switchView {
  // Update the item.
  _storageSwitchItem.on = switchView.on;
}

- (void)onNewCardLinkTapped:(UIButton*)button {
  _bridge->GetController()->NewCardLinkClicked();
  _CVCItem.instructionsText = base::SysUTF16ToNSString(
      _bridge->GetController()->GetInstructionsMessage());
  _CVCItem.monthText = @"";
  _CVCItem.yearText = @"";
  _CVCItem.CVCText = @"";
  _CVCItem.errorMessage = @"";
  _CVCItem.showDateInput = YES;
  _CVCItem.showNewCardButton = NO;
  _CVCItem.showDateInputError = NO;
  _CVCItem.showCVCInputError = NO;

  [self reconfigureCellsForItems:@[ _CVCItem ]];
  [self.collectionViewLayout invalidateLayout];

  [self inputsDidChange:_CVCItem];
}

#pragma mark - UITextField Events

- (void)monthInputDidChange:(UITextField*)textField {
  _CVCItem.monthText = textField.text;
  [self inputsDidChange:_CVCItem];
  [self updateDateErrorState:_CVCItem];
}

- (void)yearInputDidChange:(UITextField*)textField {
  _CVCItem.yearText = textField.text;
  [self inputsDidChange:_CVCItem];
  [self updateDateErrorState:_CVCItem];
}

- (void)CVCInputDidChange:(UITextField*)textField {
  _CVCItem.CVCText = textField.text;
  [self inputsDidChange:_CVCItem];
  if (_bridge->GetController()->InputCvcIsValid(
          base::SysNSStringToUTF16(textField.text))) {
    _CVCItem.showCVCInputError = NO;
    [self updateDateErrorState:_CVCItem];
  }
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  if (item.type == ItemTypeStatus) {
    return [self statusCellHeight];
  }

  UIEdgeInsets inset = [self collectionView:collectionView
                                     layout:collectionView.collectionViewLayout
                     insetForSectionAtIndex:indexPath.section];
  return [MDCCollectionViewCell
      cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds) -
                                 inset.left - inset.right
                         forItem:item];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  return YES;
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  ItemType itemType = static_cast<ItemType>(
      [self.collectionViewModel itemTypeForIndexPath:indexPath]);
  switch (itemType) {
    case ItemTypeCVC: {
      CVCCell* cellForCVC = base::mac::ObjCCastStrict<CVCCell>(cell);
      [cellForCVC.monthInput addTarget:self
                                action:@selector(monthInputDidChange:)
                      forControlEvents:UIControlEventEditingChanged];
      [cellForCVC.yearInput addTarget:self
                               action:@selector(yearInputDidChange:)
                     forControlEvents:UIControlEventEditingChanged];
      [cellForCVC.CVCInput addTarget:self
                              action:@selector(CVCInputDidChange:)
                    forControlEvents:UIControlEventEditingChanged];
      [cellForCVC.buttonForNewCard addTarget:self
                                      action:@selector(onNewCardLinkTapped:)
                            forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeStorageSwitch: {
      StorageSwitchCell* storageSwitchCell =
          base::mac::ObjCCastStrict<StorageSwitchCell>(cell);
      [storageSwitchCell.tooltipButton
                 addTarget:self
                    action:@selector(onTooltipButtonTapped:)
          forControlEvents:UIControlEventTouchUpInside];
      [storageSwitchCell.switchView addTarget:self
                                       action:@selector(onStorageSwitchChanged:)
                             forControlEvents:UIControlEventValueChanged];
      break;
    }
    default:
      break;
  }
  return cell;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
       willDisplayCell:(UICollectionViewCell*)cell
    forItemAtIndexPath:(NSIndexPath*)indexPath {
  CVCCell* CVC = base::mac::ObjCCast<CVCCell>(cell);
  if (CVC) {
    [self focusInputIfNeeded:CVC];
  }
}

@end
