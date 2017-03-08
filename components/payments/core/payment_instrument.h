// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_INSTRUMENT_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_INSTRUMENT_H_

#include <set>
#include <string>

#include "base/macros.h"

namespace payments {

// Base class which represents a form of payment in Payment Request.
class PaymentInstrument {
 public:
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

  virtual ~PaymentInstrument() {}

  // Will call into the |delegate| (can't be null) on success or error.
  virtual void InvokePaymentApp(Delegate* delegate) = 0;

  const std::string& method_name() { return method_name_; }

 protected:
  explicit PaymentInstrument(const std::string& method_name)
      : method_name_(method_name) {}

 private:
  const std::string method_name_;

  DISALLOW_COPY_AND_ASSIGN(PaymentInstrument);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_INSTRUMENT_H_
