// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_EDITOR_FIELD_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_EDITOR_FIELD_H_

#import <Foundation/Foundation.h>

@class AutofillEditItem;

// Field definition for an editor field. Used for building the UI and
// validation.
@interface EditorField : NSObject

// Autofill type for the field. Corresponds to autofill::ServerFieldType
@property(nonatomic, assign) NSInteger autofillType;
// Label for the field.
@property(nonatomic, copy) NSString* label;
// Value of the field. May be nil.
@property(nonatomic, copy) NSString* value;
// Whether the field is required.
@property(nonatomic, getter=isRequired) BOOL required;
// The associated AutofillEditItem instance. May be nil.
@property(nonatomic, assign) AutofillEditItem* item;
// The section identifier for the associated AutofillEditItem.
@property(nonatomic, assign) NSInteger sectionIdentifier;

- (instancetype)initWithAutofillType:(NSInteger)autofillType
                               label:(NSString*)label
                               value:(NSString*)value
                            required:(BOOL)required;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_EDITOR_FIELD_H_
