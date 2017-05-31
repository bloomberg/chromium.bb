// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_EDITOR_FIELD_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_EDITOR_FIELD_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"

@class CollectionViewItem;

// Type of the editor field. i.e., text field or selector field.
typedef NS_ENUM(NSInteger, EditorFieldType) {
  EditorFieldTypeTextField,
  EditorFieldTypeSelector,
  EditorFieldTypeSwitch,
};

// Field definition for an editor field. Used for building the UI and
// validation.
@interface EditorField : NSObject

// Autofill type for the field.
@property(nonatomic, assign) AutofillUIType autofillUIType;
// Type of the field.
@property(nonatomic, assign) EditorFieldType fieldType;
// Label for the field.
@property(nonatomic, copy) NSString* label;
// Value of the field. May be nil.
@property(nonatomic, copy) NSString* value;
// Optional display value. Used in selector editor fields where |value| is not
// meant for display purposes.
@property(nonatomic, copy) NSString* displayValue;
// Whether the field is required.
@property(nonatomic, getter=isRequired) BOOL required;
// Whether the field is enabled.
@property(nonatomic, getter=isEnabled) BOOL enabled;
// The associated CollectionViewItem instance. May be nil.
@property(nonatomic, strong) CollectionViewItem* item;
// The section identifier for the associated AutofillEditItem.
@property(nonatomic, assign) NSInteger sectionIdentifier;

- (instancetype)initWithAutofillUIType:(AutofillUIType)autofillUIType
                             fieldType:(EditorFieldType)fieldType
                                 label:(NSString*)label
                                 value:(NSString*)value
                              required:(BOOL)required;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_EDITOR_FIELD_H_
