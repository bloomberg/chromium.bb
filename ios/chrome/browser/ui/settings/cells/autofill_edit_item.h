// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_AUTOFILL_EDIT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_AUTOFILL_EDIT_ITEM_H_

#import <UIKit/UIKit.h>

#include "components/autofill/core/browser/autofill_profile.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// Item to represent and configure an AutofillEditItem. It features a label and
// a text field.
@interface AutofillEditItem : CollectionViewItem

// The name of the text field.
@property(nonatomic, copy) NSString* textFieldName;

// The value of the text field.
@property(nonatomic, copy) NSString* textFieldValue;

// The field type this item is describing.
@property(nonatomic, assign) autofill::ServerFieldType autofillType;

// Whether the text field is enabled for editing.
@property(nonatomic, getter=isTextFieldEnabled) BOOL textFieldEnabled;

@end

// AutofillEditCell implements an MDCCollectionViewCell subclass containing a
// label and a text field.
@interface AutofillEditCell : MDCCollectionViewCell

// Label at the leading edge of the cell. It displays the item's textFieldName.
@property(nonatomic, strong) UILabel* textLabel;

// Text field at the trailing edge of the cell. It displays the item's
// |textFieldValue|.
@property(nonatomic, readonly, strong) UITextField* textField;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_AUTOFILL_EDIT_ITEM_H_
