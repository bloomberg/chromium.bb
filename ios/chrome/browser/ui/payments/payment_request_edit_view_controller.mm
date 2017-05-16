// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/autofill/autofill_edit_accessory_view.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item+collection_view_controller.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/payments/cells/payments_selector_edit_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller+internal.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kWarningMessageAccessibilityID =
    @"kWarningMessageAccessibilityID";

namespace {

NSString* const kPaymentRequestEditCollectionViewAccessibilityID =
    @"kPaymentRequestEditCollectionViewAccessibilityID";

const CGFloat kSeparatorEdgeInset = 14;

const CGFloat kFooterCellHorizontalPadding = 16;

// Returns the AutofillEditCell that is the parent view of the |textField|.
AutofillEditCell* AutofillEditCellForTextField(UITextField* textField) {
  for (UIView* view = textField; view; view = [view superview]) {
    AutofillEditCell* cell = base::mac::ObjCCast<AutofillEditCell>(view);
    if (cell)
      return cell;
  }

  // There has to be a cell associated with this text field.
  NOTREACHED();
  return nil;
}

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierHeader = kSectionIdentifierEnumZero,
  SectionIdentifierFooter,
  SectionIdentifierFirstField,  // Must be the last section identifier.
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeFooter,
  ItemTypeTextField,      // This is a repeated item type.
  ItemTypeSelectorField,  // This is a repeated item type.
  ItemTypeErrorMessage,   // This is a repeated item type.
};

}  // namespace

@interface PaymentRequestEditViewController ()<
    AutofillEditAccessoryDelegate,
    UITextFieldDelegate> {
  // The currently focused cell. May be nil.
  __weak AutofillEditCell* _currentEditingCell;

  AutofillEditAccessoryView* _accessoryView;
}

// The list of field definitions for the editor.
@property(nonatomic, strong) NSArray<EditorField*>* fields;

// Returns the indexPath for the same row as that of |indexPath| in a section
// with the given offset relative to that of |indexPath|. May return nil.
- (NSIndexPath*)indexPathWithSectionOffset:(NSInteger)offset
                                  fromPath:(NSIndexPath*)indexPath;

// Returns the text field with the given offset relative to the currently
// focused text field. May return nil.
- (AutofillEditCell*)nextTextFieldWithOffset:(NSInteger)offset;

// Enables or disables the accessory view's previous and next buttons depending
// on whether there is a text field before and after the currently focused text
// field.
- (void)updateAccessoryViewButtonsStates;

// Adds an error message item in the section |sectionIdentifier| if
// |errorMessage| is non-empty. Otherwise removes such an item if one exists.
- (void)addOrRemoveErrorMessage:(NSString*)errorMessage
        inSectionWithIdentifier:(NSInteger)sectionIdentifier;

@end

@implementation PaymentRequestEditViewController

@synthesize dataSource = _dataSource;
@synthesize delegate = _delegate;
@synthesize validatorDelegate = _validatorDelegate;
@synthesize fields = _fields;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style {
  self = [super initWithStyle:style];
  if (self) {
    _accessoryView = [[AutofillEditAccessoryView alloc] initWithDelegate:self];
  }
  return self;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardDidShow)
             name:UIKeyboardDidShowNotification
           object:nil];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIKeyboardDidShowNotification
              object:nil];
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  CollectionViewItem* headerItem = [_dataSource headerItem];
  if (headerItem) {
    [headerItem setType:ItemTypeHeader];
    [model addSectionWithIdentifier:SectionIdentifierHeader];
    [model addItem:headerItem toSectionWithIdentifier:SectionIdentifierHeader];
  }

  // Iterate over the fields and add the respective sections and items.
  int sectionIdentifier = static_cast<int>(SectionIdentifierFirstField);
  for (EditorField* field in self.fields) {
    [model addSectionWithIdentifier:sectionIdentifier];
    switch (field.fieldType) {
      case EditorFieldTypeTextField: {
        AutofillEditItem* item =
            [[AutofillEditItem alloc] initWithType:ItemTypeTextField];
        item.textFieldName = field.label;
        item.textFieldEnabled = YES;
        item.textFieldValue = field.value;
        item.required = field.isRequired;
        item.autofillUIType = field.autofillUIType;
        [model addItem:item
            toSectionWithIdentifier:static_cast<NSInteger>(sectionIdentifier)];
        field.item = item;
        break;
      }
      case EditorFieldTypeSelector: {
        PaymentsSelectorEditItem* item = [[PaymentsSelectorEditItem alloc]
            initWithType:ItemTypeSelectorField];
        item.name = field.label;
        item.value = field.displayValue;
        item.required = field.isRequired;
        item.autofillUIType = field.autofillUIType;
        item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
        [model addItem:item
            toSectionWithIdentifier:static_cast<NSInteger>(sectionIdentifier)];
        field.item = item;
        break;
      }
      default:
        NOTREACHED();
    }

    field.sectionIdentifier = static_cast<NSInteger>(sectionIdentifier);
    ++sectionIdentifier;
  }

  [self loadFooterItems];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.collectionView.accessibilityIdentifier =
      kPaymentRequestEditCollectionViewAccessibilityID;

  // Customize collection view settings.
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.separatorInset =
      UIEdgeInsetsMake(0, kSeparatorEdgeInset, 0, kSeparatorEdgeInset);
}

#pragma mark - PaymentRequestEditConsumer

- (void)setEditorFields:(NSArray<EditorField*>*)fields {
  self.fields = fields;
}

#pragma mark - UITextFieldDelegate

- (void)textFieldDidBeginEditing:(UITextField*)textField {
  _currentEditingCell = AutofillEditCellForTextField(textField);
  [textField setInputAccessoryView:_accessoryView];
  [self updateAccessoryViewButtonsStates];
}

- (void)textFieldDidEndEditing:(UITextField*)textField {
  DCHECK(_currentEditingCell == AutofillEditCellForTextField(textField));

  // Validate the text field.
  CollectionViewModel* model = self.collectionViewModel;

  NSIndexPath* indexPath = [self indexPathForCurrentTextField];
  AutofillEditItem* item = base::mac::ObjCCastStrict<AutofillEditItem>(
      [model itemAtIndexPath:indexPath]);

  // Create a dummy EditorField for validation only.
  EditorField* fieldForValidation =
      [[EditorField alloc] initWithAutofillUIType:item.autofillUIType
                                        fieldType:EditorFieldTypeTextField
                                            label:nil
                                            value:textField.text
                                         required:item.required];
  NSString* errorMessage =
      [_validatorDelegate paymentRequestEditViewController:self
                                             validateField:fieldForValidation];
  NSInteger sectionIdentifier =
      [model sectionIdentifierForSection:[indexPath section]];
  [self addOrRemoveErrorMessage:errorMessage
        inSectionWithIdentifier:sectionIdentifier];

  [textField setInputAccessoryView:nil];
  _currentEditingCell = nil;
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  DCHECK([_currentEditingCell textField] == textField);
  AutofillEditCell* nextCell = [self nextTextFieldWithOffset:1];
  if (nextCell)
    [self nextPressed];
  else
    [self closePressed];

  return NO;
}

#pragma mark - AutofillEditAccessoryDelegate

- (void)nextPressed {
  AutofillEditCell* nextCell = [self nextTextFieldWithOffset:1];
  if (nextCell)
    [nextCell.textField becomeFirstResponder];
}

- (void)previousPressed {
  AutofillEditCell* previousCell = [self nextTextFieldWithOffset:-1];
  if (previousCell)
    [previousCell.textField becomeFirstResponder];
}

- (void)closePressed {
  [[_currentEditingCell textField] resignFirstResponder];
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeTextField: {
      AutofillEditCell* autofillEditCell =
          base::mac::ObjCCast<AutofillEditCell>(cell);
      autofillEditCell.textField.delegate = self;
      autofillEditCell.textField.clearButtonMode = UITextFieldViewModeNever;
      autofillEditCell.textLabel.font = [MDCTypography body2Font];
      autofillEditCell.textLabel.textColor = [[MDCPalette greyPalette] tint900];
      autofillEditCell.textField.font = [MDCTypography body1Font];
      autofillEditCell.textField.textColor =
          [[MDCPalette cr_bluePalette] tint600];
      break;
    }
    case ItemTypeErrorMessage: {
      PaymentsTextCell* errorMessageCell =
          base::mac::ObjCCastStrict<PaymentsTextCell>(cell);
      errorMessageCell.textLabel.font = [MDCTypography body1Font];
      errorMessageCell.textLabel.textColor =
          [[MDCPalette cr_redPalette] tint600];
      break;
    }
    case ItemTypeFooter: {
      CollectionViewFooterCell* footerCell =
          base::mac::ObjCCastStrict<CollectionViewFooterCell>(cell);
      footerCell.textLabel.font = [MDCTypography body2Font];
      footerCell.textLabel.textColor = [[MDCPalette greyPalette] tint600];
      footerCell.textLabel.shadowColor = nil;  // No shadow.
      footerCell.horizontalPadding = kFooterCellHorizontalPadding;
      break;
    }
    default:
      break;
  }

  return cell;
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  // Every field has its own section. Find out which field is selected using
  // the section of |indexPath|. Adjust the index if a header section is
  // present before the editor fields.
  NSInteger index = indexPath.section;
  if ([self.collectionViewModel
          hasSectionForSectionIdentifier:SectionIdentifierHeader])
    index--;
  DCHECK(index >= 0 && index < static_cast<NSInteger>(self.fields.count));
  [_delegate
      paymentRequestEditViewController:self
                        didSelectField:[self.fields objectAtIndex:index]];
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeHeader:
    case ItemTypeFooter:
    case ItemTypeTextField:
    case ItemTypeErrorMessage:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    case ItemTypeSelectorField:
      return MDCCellDefaultOneLineHeight;
    default:
      NOTREACHED();
      return MDCCellDefaultOneLineHeight;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeHeader:
    case ItemTypeFooter:
    case ItemTypeErrorMessage:
      return YES;
    default:
      return NO;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeHeader:
      return [_dataSource shouldHideBackgroundForHeaderItem];
    case ItemTypeFooter:
      return YES;
    default:
      return NO;
  }
}

#pragma mark - Helper methods

- (NSIndexPath*)indexPathWithSectionOffset:(NSInteger)offset
                                  fromPath:(NSIndexPath*)indexPath {
  DCHECK(indexPath);
  DCHECK(offset);
  NSInteger nextSection = [indexPath section] + offset;
  if (nextSection >= 0 &&
      nextSection < [[self collectionView] numberOfSections]) {
    return [NSIndexPath indexPathForRow:[indexPath row] inSection:nextSection];
  }
  return nil;
}

- (AutofillEditCell*)nextTextFieldWithOffset:(NSInteger)offset {
  UICollectionView* collectionView = [self collectionView];
  NSIndexPath* currentCellPath = [self indexPathForCurrentTextField];
  DCHECK(currentCellPath);
  NSIndexPath* nextCellPath =
      [self indexPathWithSectionOffset:offset fromPath:currentCellPath];
  while (nextCellPath) {
    id nextCell = [collectionView cellForItemAtIndexPath:nextCellPath];
    if ([nextCell isKindOfClass:[AutofillEditCell class]]) {
      return base::mac::ObjCCastStrict<AutofillEditCell>(
          [collectionView cellForItemAtIndexPath:nextCellPath]);
    }
    nextCellPath =
        [self indexPathWithSectionOffset:offset fromPath:nextCellPath];
  }
  return nil;
}

- (void)updateAccessoryViewButtonsStates {
  AutofillEditCell* previousCell = [self nextTextFieldWithOffset:-1];
  [[_accessoryView previousButton] setEnabled:previousCell != nil];

  AutofillEditCell* nextCell = [self nextTextFieldWithOffset:1];
  [[_accessoryView nextButton] setEnabled:nextCell != nil];
}

- (void)addOrRemoveErrorMessage:(NSString*)errorMessage
        inSectionWithIdentifier:(NSInteger)sectionIdentifier {
  CollectionViewModel* model = self.collectionViewModel;
  if ([model hasItemForItemType:ItemTypeErrorMessage
              sectionIdentifier:sectionIdentifier]) {
    NSIndexPath* indexPath = [model indexPathForItemType:ItemTypeErrorMessage
                                       sectionIdentifier:sectionIdentifier];
    if (!errorMessage.length) {
      // Remove the item at the index path.
      [model removeItemWithType:ItemTypeErrorMessage
          fromSectionWithIdentifier:sectionIdentifier];
      [self.collectionView deleteItemsAtIndexPaths:@[ indexPath ]];
    } else {
      // Reload the item at the index path.
      PaymentsTextItem* item = base::mac::ObjCCastStrict<PaymentsTextItem>(
          [model itemAtIndexPath:indexPath]);
      item.text = errorMessage;
      [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
    }
  } else if (errorMessage.length) {
    // Insert an item at the index path.
    PaymentsTextItem* errorMessageItem =
        [[PaymentsTextItem alloc] initWithType:ItemTypeErrorMessage];
    errorMessageItem.text = errorMessage;
    errorMessageItem.image = NativeImage(IDR_IOS_PAYMENTS_WARNING);
    errorMessageItem.accessibilityIdentifier = kWarningMessageAccessibilityID;
    [model addItem:errorMessageItem toSectionWithIdentifier:sectionIdentifier];
    NSIndexPath* indexPath = [model indexPathForItemType:ItemTypeErrorMessage
                                       sectionIdentifier:sectionIdentifier];
    [self.collectionView insertItemsAtIndexPaths:@[ indexPath ]];
  }
}

#pragma mark - Keyboard handling

- (void)keyboardDidShow {
  [self.collectionView
      scrollToItemAtIndexPath:[self.collectionView
                                  indexPathForCell:_currentEditingCell]
             atScrollPosition:UICollectionViewScrollPositionCenteredVertically
                     animated:YES];
}

@end

@implementation PaymentRequestEditViewController (Internal)

- (BOOL)validateForm {
  for (EditorField* field in self.fields) {
    switch (field.fieldType) {
      case EditorFieldTypeTextField: {
        AutofillEditItem* item =
            base::mac::ObjCCastStrict<AutofillEditItem>(field.item);
        // Update the EditorField with the value of the text field.
        field.value = item.textFieldValue;
        break;
      }
      case EditorFieldTypeSelector: {
        // No need to update the EditorField. It should already be up-to-date.
        break;
      }
      default:
        NOTREACHED();
    }

    NSString* errorMessage =
        [_validatorDelegate paymentRequestEditViewController:self
                                               validateField:field];
    [self addOrRemoveErrorMessage:errorMessage
          inSectionWithIdentifier:field.sectionIdentifier];
    if (errorMessage.length)
      return NO;
  }
  return YES;
}

- (void)loadFooterItems {
  CollectionViewModel* model = self.collectionViewModel;

  [model addSectionWithIdentifier:SectionIdentifierFooter];
  CollectionViewFooterItem* footerItem =
      [[CollectionViewFooterItem alloc] initWithType:ItemTypeFooter];
  footerItem.text = l10n_util::GetNSString(IDS_PAYMENTS_REQUIRED_FIELD_MESSAGE);
  [model addItem:footerItem toSectionWithIdentifier:SectionIdentifierFooter];
}

- (NSIndexPath*)indexPathForCurrentTextField {
  DCHECK(_currentEditingCell);
  NSIndexPath* indexPath =
      [[self collectionView] indexPathForCell:_currentEditingCell];
  DCHECK(indexPath);
  return indexPath;
}

@end
