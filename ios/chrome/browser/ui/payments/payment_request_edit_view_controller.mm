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
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/payments/cells/payments_selector_edit_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller_actions.h"
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

CollectionViewSwitchCell* CollectionViewSwitchCellForSwitchField(
    UISwitch* switchField) {
  for (UIView* view = switchField; view; view = [view superview]) {
    CollectionViewSwitchCell* cell =
        base::mac::ObjCCast<CollectionViewSwitchCell>(view);
    if (cell)
      return cell;
  }

  // There should be a cell associated with this switch field.
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
  ItemTypeSwitchField,    // This is a repeated item type.
  ItemTypeErrorMessage,   // This is a repeated item type.
};

}  // namespace

@interface PaymentRequestEditViewController ()<
    AutofillEditAccessoryDelegate,
    PaymentRequestEditViewControllerActions,
    UIPickerViewDataSource,
    UIPickerViewDelegate,
    UITextFieldDelegate> {
  // The currently focused cell. May be nil.
  __weak AutofillEditCell* _currentEditingCell;

  AutofillEditAccessoryView* _accessoryView;
}

// The map of section identifiers to the fields definitions for the editor.
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, EditorField*>* fieldsMap;

// The list of field definitions for the editor.
@property(nonatomic, strong) NSArray<EditorField*>* fields;

// The map of autofill types to lists of UIPickerView options.
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, NSArray<NSString*>*>* options;

// The map of autofill types to UIPickerView views.
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, UIPickerView*>* pickerViews;

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

// Validates each field. If there is a validation error, displays an error
// message item in the same section as the field and returns NO. Otherwise
// removes the error message item in that section if one exists and sets the
// value on the field. Returns YES if all the fields are validated successfully.
- (BOOL)validateForm;

// Returns the index path for the cell associated with the currently focused
// text field.
- (NSIndexPath*)indexPathForCurrentTextField;

@end

@implementation PaymentRequestEditViewController

@synthesize dataSource = _dataSource;
@synthesize delegate = _delegate;
@synthesize validatorDelegate = _validatorDelegate;
@synthesize fieldsMap = _fieldsMap;
@synthesize fields = _fields;
@synthesize options = _options;
@synthesize pickerViews = _pickerViews;

- (instancetype)init {
  self = [self initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    // Set up leading (cancel) button.
    UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                style:UIBarButtonItemStylePlain
               target:nil
               action:@selector(onCancel)];
    [cancelButton setTitleTextAttributes:@{
      NSForegroundColorAttributeName : [UIColor lightGrayColor]
    }
                                forState:UIControlStateDisabled];
    [cancelButton
        setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_CANCEL)];
    [self navigationItem].leftBarButtonItem = cancelButton;

    // Set up trailing (done) button.
    UIBarButtonItem* doneButton =
        [[UIBarButtonItem alloc] initWithTitle:l10n_util::GetNSString(IDS_DONE)
                                         style:UIBarButtonItemStylePlain
                                        target:nil
                                        action:@selector(onDone)];
    [doneButton setTitleTextAttributes:@{
      NSForegroundColorAttributeName : [UIColor lightGrayColor]
    }
                              forState:UIControlStateDisabled];
    [doneButton setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_DONE)];
    [self navigationItem].rightBarButtonItem = doneButton;
  }

  return self;
}

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style {
  self = [super initWithStyle:style];
  if (self) {
    _accessoryView = [[AutofillEditAccessoryView alloc] initWithDelegate:self];
    _options = [[NSMutableDictionary alloc] init];
    _pickerViews = [[NSMutableDictionary alloc] init];
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

  [self.pickerViews removeAllObjects];

  CollectionViewItem* headerItem = [_dataSource headerItem];
  if (headerItem) {
    [headerItem setType:ItemTypeHeader];
    [model addSectionWithIdentifier:SectionIdentifierHeader];
    [model addItem:headerItem toSectionWithIdentifier:SectionIdentifierHeader];
  }

  self.fieldsMap =
      [[NSMutableDictionary alloc] initWithCapacity:self.fields.count];

  // Iterate over the fields and add the respective sections and items.
  [self.fields enumerateObjectsUsingBlock:^(EditorField* field,
                                            NSUInteger index, BOOL* stop) {
    NSInteger sectionIdentifier = SectionIdentifierFirstField + index;
    [model addSectionWithIdentifier:sectionIdentifier];
    switch (field.fieldType) {
      case EditorFieldTypeTextField: {
        AutofillEditItem* item =
            [[AutofillEditItem alloc] initWithType:ItemTypeTextField];
        item.textFieldName = field.label;
        item.textFieldEnabled = field.enabled;
        item.textFieldValue = field.value;
        item.required = field.isRequired;
        item.autofillUIType = field.autofillUIType;
        item.identifyingIcon = [_dataSource iconIdentifyingEditorField:field];
        [model addItem:item toSectionWithIdentifier:sectionIdentifier];
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
        [model addItem:item toSectionWithIdentifier:sectionIdentifier];
        field.item = item;
        break;
      }
      case EditorFieldTypeSwitch: {
        CollectionViewSwitchItem* item =
            [[CollectionViewSwitchItem alloc] initWithType:ItemTypeSwitchField];
        item.text = field.label;
        item.on = [field.value boolValue];
        [model addItem:item toSectionWithIdentifier:sectionIdentifier];
        field.item = item;
        break;
      }
      default:
        NOTREACHED();
    }

    field.sectionIdentifier = sectionIdentifier;
    NSNumber* key = [NSNumber numberWithInt:sectionIdentifier];
    [self.fieldsMap setObject:field forKey:key];
  }];

  [model addSectionWithIdentifier:SectionIdentifierFooter];
  CollectionViewFooterItem* footerItem =
      [[CollectionViewFooterItem alloc] initWithType:ItemTypeFooter];
  footerItem.text = l10n_util::GetNSString(IDS_PAYMENTS_REQUIRED_FIELD_MESSAGE);
  [model addItem:footerItem toSectionWithIdentifier:SectionIdentifierFooter];
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

- (void)setOptions:(NSArray<NSString*>*)options
    forEditorField:(EditorField*)field {
  DCHECK(field.fieldType == EditorFieldTypeTextField);
  AutofillEditItem* item =
      base::mac::ObjCCastStrict<AutofillEditItem>(field.item);
  item.textFieldEnabled = field.enabled;
  item.textFieldValue = field.value;

  // Cache the options if there are any and set the text field's UIPickerView.
  if (options.count) {
    NSNumber* key = [NSNumber numberWithInt:field.autofillUIType];
    [self.options setObject:options forKey:key];

    UIPickerView* pickerView = [[UIPickerView alloc] initWithFrame:CGRectZero];
    pickerView.delegate = self;
    pickerView.dataSource = self;
    pickerView.accessibilityIdentifier =
        [NSString stringWithFormat:@"%@_pickerView", field.label];
    [self.pickerViews setObject:pickerView forKey:key];
    item.inputView = pickerView;

    // Set UIPickerView's default selected row, if possible.
    [pickerView reloadAllComponents];
    NSUInteger indexOfRow = [options indexOfObject:field.value];
    if (indexOfRow != NSNotFound) {
      [pickerView selectRow:indexOfRow inComponent:0 animated:NO];
    }
  }

  // Reload the item.
  NSIndexPath* indexPath =
      [self.collectionViewModel indexPathForItemType:ItemTypeTextField
                                   sectionIdentifier:field.sectionIdentifier];
  [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
}

#pragma mark - UITextFieldDelegate

- (void)textFieldDidBeginEditing:(UITextField*)textField {
  _currentEditingCell = AutofillEditCellForTextField(textField);
  [textField setInputAccessoryView:_accessoryView];
  [self updateAccessoryViewButtonsStates];
}

- (void)textFieldDidEndEditing:(UITextField*)textField {
  DCHECK(_currentEditingCell == AutofillEditCellForTextField(textField));

  NSIndexPath* indexPath = [self indexPathForCurrentTextField];
  NSInteger sectionIdentifier = [self.collectionViewModel
      sectionIdentifierForSection:[indexPath section]];

  // Find the respective editor field, update its value, and validate it.
  NSNumber* key = [NSNumber numberWithInt:sectionIdentifier];
  EditorField* field = self.fieldsMap[key];
  DCHECK(field);
  field.value = textField.text;
  NSString* errorMessage =
      [_validatorDelegate paymentRequestEditViewController:self
                                             validateField:field];
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

// This method is called as the text is being typed in, pasted, or deleted. Asks
// the delegate if the text should be changed. Should always return NO. During
// typing/pasting text, |newText| contains one or more new characters. When user
// deletes text, |newText| is empty. |range| is the range of characters to be
// replaced.
- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)newText {
  CollectionViewModel* model = self.collectionViewModel;

  DCHECK(_currentEditingCell == AutofillEditCellForTextField(textField));

  NSIndexPath* indexPath = [self indexPathForCurrentTextField];
  NSInteger sectionIdentifier =
      [model sectionIdentifierForSection:[indexPath section]];
  AutofillEditItem* item = base::mac::ObjCCastStrict<AutofillEditItem>(
      [model itemAtIndexPath:indexPath]);

  // Find the respective editor field and update its value to the proposed text.
  NSNumber* key = [NSNumber numberWithInt:sectionIdentifier];
  EditorField* field = self.fieldsMap[key];
  DCHECK(field);
  field.value = [textField.text stringByReplacingCharactersInRange:range
                                                        withString:newText];

  // Format the proposed text if necessary.
  [_dataSource formatValueForEditorField:field];

  // Since this method is returning NO, update the text field's value now.
  textField.text = field.value;

  // Get the icon that identifies the field value and reload the cell if the
  // icon changes.
  UIImage* oldIcon = item.identifyingIcon;
  item.identifyingIcon = [_dataSource iconIdentifyingEditorField:field];
  if (item.identifyingIcon != oldIcon) {
    item.textFieldValue = field.value;
    [self reconfigureCellsForItems:@[ item ]];
  }

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

#pragma mark - UIPickerViewDataSource methods

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView*)thePickerView {
  return 1;
}

- (NSInteger)pickerView:(UIPickerView*)thePickerView
    numberOfRowsInComponent:(NSInteger)component {
  NSArray<NSNumber*>* indices =
      [self.pickerViews allKeysForObject:thePickerView];
  DCHECK(indices.count == 1);
  NSArray<NSString*>* options = self.options[indices[0]];
  return options.count;
}

#pragma mark - UIPickerViewDelegate methods

- (NSString*)pickerView:(UIPickerView*)thePickerView
            titleForRow:(NSInteger)row
           forComponent:(NSInteger)component {
  NSArray<NSNumber*>* indices =
      [self.pickerViews allKeysForObject:thePickerView];
  DCHECK(indices.count == 1);
  NSArray<NSString*>* options = self.options[indices[0]];
  DCHECK(row < static_cast<NSInteger>(options.count));
  return options[row];
}

- (void)pickerView:(UIPickerView*)thePickerView
      didSelectRow:(NSInteger)row
       inComponent:(NSInteger)component {
  DCHECK(_currentEditingCell);
  _currentEditingCell.textField.text =
      [self pickerView:thePickerView titleForRow:row forComponent:component];
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
    case ItemTypeSwitchField: {
      CollectionViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<CollectionViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(switchToggled:)
                      forControlEvents:UIControlEventValueChanged];
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
  EditorField* field = [self.fields objectAtIndex:index];

  // If a selector field is selected, blur the focused text field.
  if (field.fieldType == EditorFieldTypeSelector)
    [[_currentEditingCell textField] resignFirstResponder];

  if ([self.delegate respondsToSelector:@selector
                     (paymentRequestEditViewController:didSelectField:)]) {
    [_delegate paymentRequestEditViewController:self didSelectField:field];
  }
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];

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
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeHeader:
    case ItemTypeFooter:
    case ItemTypeErrorMessage:
    case ItemTypeTextField:
    case ItemTypeSwitchField:
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

- (BOOL)validateForm {
  for (EditorField* field in self.fields) {
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

- (NSIndexPath*)indexPathForCurrentTextField {
  DCHECK(_currentEditingCell);
  NSIndexPath* indexPath =
      [[self collectionView] indexPathForCell:_currentEditingCell];
  DCHECK(indexPath);
  return indexPath;
}

#pragma mark - Keyboard handling

- (void)keyboardDidShow {
  [self.collectionView
      scrollToItemAtIndexPath:[self.collectionView
                                  indexPathForCell:_currentEditingCell]
             atScrollPosition:UICollectionViewScrollPositionCenteredVertically
                     animated:YES];
}

#pragma mark Switch Actions

- (void)switchToggled:(UISwitch*)sender {
  CollectionViewSwitchCell* switchCell =
      CollectionViewSwitchCellForSwitchField(sender);
  NSIndexPath* indexPath = [[self collectionView] indexPathForCell:switchCell];
  DCHECK(indexPath);

  NSInteger sectionIdentifier = [self.collectionViewModel
      sectionIdentifierForSection:[indexPath section]];

  // Update editor field's value.
  NSNumber* key = [NSNumber numberWithInt:sectionIdentifier];
  EditorField* field = self.fieldsMap[key];
  DCHECK(field);
  field.value = [sender isOn] ? @"YES" : @"NO";
}

#pragma mark - PaymentRequestEditViewControllerActions methods

- (void)onCancel {
  [self.delegate paymentRequestEditViewControllerDidCancel:self];
}

- (void)onDone {
  [_currentEditingCell.textField resignFirstResponder];

  if (![self validateForm])
    return;

  [self.delegate paymentRequestEditViewController:self
                           didFinishEditingFields:self.fields];
}

@end
