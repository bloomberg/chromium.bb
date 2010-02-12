// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_CREDIT_CARD_MODEL_MAC_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_CREDIT_CARD_MODEL_MAC_

#import <Cocoa/Cocoa.h>

class CreditCard;

// A "model" class used with bindings mechanism and the
// |AutoFillCreditCardViewController| to achieve the form-like view
// of autofill data in the Chrome options UI.
// Note that |summary| is a derived property.
// Model objects are initialized from the given |creditCard| using the
// designated initializer |initWithCreditCard:|.
// Users of this class must be prepared to handle nil string return values.
// The KVO/bindings mechanisms expect this and deal with nil string values
// appropriately.
@interface AutoFillCreditCardModel : NSObject {
 @private
  // These are not scoped_nsobjects because we use them via KVO/bindings.
  NSString* label_;
  NSString* nameOnCard_;
  NSString* creditCardNumber_;
  NSString* expirationMonth_;
  NSString* expirationYear_;
  NSString* cvcCode_;
  NSString* billingAddress_;
  NSString* shippingAddress_;
}

// |summary| is a derived property based on |creditCardNumber|,
// |expirationMonth| and |expirationYear|.  KVO observers receive change
// notifications for |summary| when any of these properties change.
@property (readonly) NSString* summary;
@property (nonatomic, copy) NSString* label;
@property (nonatomic, copy) NSString* nameOnCard;
@property (nonatomic, copy) NSString* creditCardNumber;
@property (nonatomic, copy) NSString* expirationMonth;
@property (nonatomic, copy) NSString* expirationYear;
@property (nonatomic, copy) NSString* cvcCode;
@property (nonatomic, copy) NSString* billingAddress;
@property (nonatomic, copy) NSString* shippingAddress;

// Designated initializer.  Initializes the property strings to values retrieved
// from the |creditCard| object.
- (id)initWithCreditCard:(const CreditCard&)creditCard;

// This method copies internal NSString property values into the
// |creditCard| object's fields as appropriate.  |creditCard| should never
// be NULL.
- (void)copyModelToCreditCard:(CreditCard*)creditCard;

@end

#endif // CHROME_BROWSER_AUTOFILL_AUTOFILL_CREDIT_CARD_MODEL_MAC_
