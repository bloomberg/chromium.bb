// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_INSTRUMENT_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_INSTRUMENT_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace payments {

// Base class which represents a form of payment in Payment Request.
class PaymentInstrument {
 public:
  // The type of this instrument instance.
  enum class Type { AUTOFILL };

  class Delegate {
   public:
    virtual ~Delegate() {}

    // Should be called with method name (e.g., "visa") and json-serialized
    // stringified details.
    virtual void OnInstrumentDetailsReady(
        const std::string& method_name,
        const std::string& stringified_details) = 0;

    virtual void OnInstrumentDetailsError() = 0;
  };

  virtual ~PaymentInstrument();

  // Will call into the |delegate| (can't be null) on success or error.
  virtual void InvokePaymentApp(Delegate* delegate) = 0;
  // Returns whether the instrument is complete to be used as a payment method
  // without further editing.
  virtual bool IsCompleteForPayment() = 0;
  // Returns whether the instrument is exactly matching all filters provided by
  // the merchant. For example, this can return "false" for unknown card types,
  // if the merchant requested only debit cards.
  virtual bool IsExactlyMatchingMerchantRequest() = 0;
  // Returns a message to indicate to the user what's missing for the instrument
  // to be complete for payment.
  virtual base::string16 GetMissingInfoLabel() = 0;
  // Returns whether the instrument is valid for the purposes of responding to
  // canMakePayment.
  virtual bool IsValidForCanMakePayment() = 0;
  // Records the use of this payment instrument.
  virtual void RecordUse() = 0;
  // Return the sub/label of payment instrument, to be displayed to the user.
  virtual base::string16 GetLabel() const = 0;
  virtual base::string16 GetSublabel() const = 0;

  const std::string& method_name() const { return method_name_; }
  int icon_resource_id() const { return icon_resource_id_; }
  Type type() { return type_; }

 protected:
  PaymentInstrument(const std::string& method_name,
                    int icon_resource_id,
                    Type type);

 private:
  const std::string method_name_;
  int icon_resource_id_;
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(PaymentInstrument);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_INSTRUMENT_H_
