// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DELEGATE_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

namespace i18n {
namespace addressinput {
class Storage;
class Source;
}  // namespace addressinput
}  // namespace i18n

namespace autofill {
class CreditCard;
class PersonalDataManager;
}  // namespace autofill

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

  // Disables the dialog and shows an error message that the transaction has
  // failed.
  virtual void ShowErrorMessage() = 0;

  // Gets the PersonalDataManager associated with this PaymentRequest flow.
  // Cannot be null.
  virtual autofill::PersonalDataManager* GetPersonalDataManager() = 0;

  virtual const std::string& GetApplicationLocale() const = 0;

  // Returns whether the user is in Incognito mode.
  virtual bool IsIncognito() const = 0;

  // Starts a FullCardRequest to unmask |credit_card|.
  virtual void DoFullCardRequest(
      const autofill::CreditCard& credit_card,
      base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
          result_delegate) = 0;

  // Returns the source and storage for country/region data loads.
  virtual std::unique_ptr<const ::i18n::addressinput::Source>
  GetAddressInputSource() = 0;
  virtual std::unique_ptr<::i18n::addressinput::Storage>
  GetAddressInputStorage() = 0;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DELEGATE_H_
