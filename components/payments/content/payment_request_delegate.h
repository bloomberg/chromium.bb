// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DELEGATE_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DELEGATE_H_

#include <string>

namespace autofill {
class PersonalDataManager;
}

namespace payments {

class PaymentRequest;

class PaymentRequestDelegate {
 public:
  virtual ~PaymentRequestDelegate() {}

  // Shows the Payment Request dialog for the given |request|.
  virtual void ShowDialog(PaymentRequest* request) = 0;

  // Closes the same dialog that was opened by this delegate. Must be safe to
  // call when the dialog is not showing.
  virtual void CloseDialog() = 0;

  // Gets the PersonalDataManager associated with this PaymentRequest flow.
  // Cannot be null.
  virtual autofill::PersonalDataManager* GetPersonalDataManager() = 0;

  virtual const std::string& GetApplicationLocale() const = 0;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DELEGATE_H_
