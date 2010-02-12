// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_ADDRESS_MODEL_MAC_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_ADDRESS_MODEL_MAC_

#import <Cocoa/Cocoa.h>

class AutoFillProfile;

// A "model" class used with bindings mechanism and the
// |AutoFillAddressViewController| to achieve the form-like view
// of autofill data in the Chrome options UI.
// Note that |summary| is a derived property.
// Model objects are initialized from a given profile using the designated
// initializer |initWithProfile:|.
// Users of this class must be prepared to handle nil string return values.
// The KVO/bindings mechanisms expect this and deal with nil string values
// appropriately.
@interface AutoFillAddressModel : NSObject {
 @private
  // These are not scoped_nsobjects because we use them via KVO/bindings.
  NSString* label_;
  NSString* firstName_;
  NSString* middleName_;
  NSString* lastName_;
  NSString* email_;
  NSString* companyName_;
  NSString* addressLine1_;
  NSString* addressLine2_;
  NSString* city_;
  NSString* state_;
  NSString* zip_;
  NSString* country_;
  NSString* phoneCountryCode_;
  NSString* phoneAreaCode_;
  NSString* phoneNumber_;
  NSString* faxCountryCode_;
  NSString* faxAreaCode_;
  NSString* faxNumber_;
}

// |summary| is a derived property based on |firstName|, |lastName| and
// |addressLine1|.  KVO observers receive change notifications for |summary|
// when any of these properties change.
@property (readonly) NSString* summary;
@property (nonatomic, copy) NSString* label;
@property (nonatomic, copy) NSString* firstName;
@property (nonatomic, copy) NSString* middleName;
@property (nonatomic, copy) NSString* lastName;
@property (nonatomic, copy) NSString* email;
@property (nonatomic, copy) NSString* companyName;
@property (nonatomic, copy) NSString* addressLine1;
@property (nonatomic, copy) NSString* addressLine2;
@property (nonatomic, copy) NSString* city;
@property (nonatomic, copy) NSString* state;
@property (nonatomic, copy) NSString* zip;
@property (nonatomic, copy) NSString* country;
@property (nonatomic, copy) NSString* phoneCountryCode;
@property (nonatomic, copy) NSString* phoneAreaCode;
@property (nonatomic, copy) NSString* phoneNumber;
@property (nonatomic, copy) NSString* faxCountryCode;
@property (nonatomic, copy) NSString* faxAreaCode;
@property (nonatomic, copy) NSString* faxNumber;

// The designated initializer. Initializes the property strings to values
// retrieved from the |profile|.
- (id)initWithProfile:(const AutoFillProfile&)profile;

// This method copies internal NSString property values into the
// |profile| object's fields as appropriate.  |profile| should never be NULL.
- (void)copyModelToProfile:(AutoFillProfile*)profile;

@end

#endif // CHROME_BROWSER_AUTOFILL_AUTOFILL_ADDRESS_MODEL_MAC_
