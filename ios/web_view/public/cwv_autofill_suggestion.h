// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_SUGGESTION_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_SUGGESTION_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

CWV_EXPORT
// Represents a suggestion for a form based off of a single field.
// In the case that this suggestion is created from a credit card or address
// profile, filling using a suggestion may fill more than one field at once.
// Example:
//   If an address profile is:
//   John Doe
//   1600 Amiphtheatre Pkwy
//   Mountain View, CA 94403
//   Then if
//   <form name="shipping_info">
//     <input type='text' name="first_name"> <-- focused on this
//     <input type='text' name="last_name">
//     <input type='text' name="address_1">
//     <input type='text' name="address_2">
//     <input type='text' name="state">
//     <input type='text' name="country">
//     <input type='text' name="zip_code">
//   </form>
//   The suggestion may look like:
//   |formName| "shipping_info"
//   |fieldName| "first_name"
//   |value| "John"
//   |displayDescription| "1600 Amphitheatre Pkwy ..."
//   Using this suggestion would replace all fields with the appropriate value.
@interface CWVAutofillSuggestion : NSObject

// The 'name' attribute of the html form element.
@property(nonatomic, copy, readonly) NSString* formName;

// The 'name' attribute of the html field element.
@property(nonatomic, copy, readonly) NSString* fieldName;

// The identifier of the html field element. If the element has an ID, it will
// be used as identifier. Otherwise, an identifier will be generated.
@property(nonatomic, copy, readonly) NSString* fieldIdentifier;

// The string that will replace the field with |fieldName|'s value attribute.
@property(nonatomic, copy, readonly) NSString* value;

// Non-nil if this suggestion is created from a credit card or address profile.
// Contain extra information from that profile to help differentiate from other
// suggestions.
@property(nonatomic, copy, readonly, nullable) NSString* displayDescription;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_SUGGESTION_H_
