// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_ADDRESS_MODEL_MAC_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_ADDRESS_MODEL_MAC_
#pragma once

#import <Cocoa/Cocoa.h>

class AutoFillProfile;

// A "model" class used with bindings mechanism and the
// |AutoFillAddressViewController| to achieve the form-like view
// of autofill data in the Chrome options UI.
// Model objects are initialized from a given profile using the designated
// initializer |initWithProfile:|.
// Users of this class must be prepared to handle nil string return values.
// The KVO/bindings mechanisms expect this and deal with nil string values
// appropriately.
@interface AutoFillAddressModel : NSObject {
 @private
  // These are not scoped_nsobjects because we use them via KVO/bindings.
  NSString* fullName_;
  NSString* email_;
  NSString* companyName_;
  NSString* addressLine1_;
  NSString* addressLine2_;
  NSString* addressCity_;
  NSString* addressState_;
  NSString* addressZip_;
  NSString* addressCountry_;
  NSString* phoneWholeNumber_;
  NSString* faxWholeNumber_;
}

@property(nonatomic, copy) NSString* fullName;
@property(nonatomic, copy) NSString* email;
@property(nonatomic, copy) NSString* companyName;
@property(nonatomic, copy) NSString* addressLine1;
@property(nonatomic, copy) NSString* addressLine2;
@property(nonatomic, copy) NSString* addressCity;
@property(nonatomic, copy) NSString* addressState;
@property(nonatomic, copy) NSString* addressZip;
@property(nonatomic, copy) NSString* addressCountry;
@property(nonatomic, copy) NSString* phoneWholeNumber;
@property(nonatomic, copy) NSString* faxWholeNumber;

// The designated initializer. Initializes the property strings to values
// retrieved from the |profile|.
- (id)initWithProfile:(const AutoFillProfile&)profile;

// This method copies internal NSString property values into the
// |profile| object's fields as appropriate.  |profile| should never be NULL.
- (void)copyModelToProfile:(AutoFillProfile*)profile;

@end

#endif // CHROME_BROWSER_AUTOFILL_AUTOFILL_ADDRESS_MODEL_MAC_
