// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_AUTOFILL_EDIT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_AUTOFILL_EDIT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// Item to represent and configure an AutofillEditItem. It features a label and
// a text field.
@interface AutofillEditItem : TableViewItem

// The name of the text field.
@property(nonatomic, copy) NSString* textFieldName;

// The value of the text field.
@property(nonatomic, copy) NSString* textFieldValue;

// An icon identifying the text field or its current value, if any.
@property(nonatomic, copy) UIImage* identifyingIcon;

// The inputView for the text field, if any.
@property(nonatomic, strong) UIPickerView* inputView;

// The field type this item is describing.
@property(nonatomic, assign) AutofillUIType autofillUIType;

// Whether this field is required. If YES, an "*" is appended to the name of the
// text field to indicate that the field is required. It is also used for
// validation purposes.
@property(nonatomic, getter=isRequired) BOOL required;

// Whether the text field is enabled for editing.
@property(nonatomic, getter=isTextFieldEnabled) BOOL textFieldEnabled;

// Controls the display of the return key when the keyboard is displaying.
@property(nonatomic, assign) UIReturnKeyType returnKeyType;

// Keyboard type to be displayed when the text field becomes first responder.
@property(nonatomic, assign) UIKeyboardType keyboardType;

// Controls autocapitalization behavior of the text field.
@property(nonatomic, assign)
    UITextAutocapitalizationType autoCapitalizationType;

@end

// AutofillEditCell implements an UITableViewCell subclass containing a label
// and a text field.
@interface AutofillEditCell : UITableViewCell

// Label at the leading edge of the cell. It displays the item's textFieldName.
@property(nonatomic, strong) UILabel* textLabel;

// Text field at the trailing edge of the cell. It displays the item's
// |textFieldValue|.
@property(nonatomic, readonly, strong) UITextField* textField;

- (void)setIdentifyingIcon:(UIImage*)icon;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_AUTOFILL_EDIT_ITEM_H_
