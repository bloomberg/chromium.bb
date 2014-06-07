// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_CLIENT_OBSERVER_H_
#define COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_CLIENT_OBSERVER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "components/autofill/content/browser/wallet/form_field_error.h"
#include "components/autofill/content/browser/wallet/wallet_client.h"
#include "components/autofill/core/browser/autofill_client.h"

class AutofillMetrics;

namespace autofill {
namespace wallet {

class FullWallet;
class WalletItems;

// WalletClientDelegate is to be implemented any classes making calls with
// WalletClient. The appropriate callback method will be called on
// WalletClientDelegate with the response from the Online Wallet backend.
class WalletClientDelegate {
 public:
  // --------------------------------------
  // Accessors called when making requests.
  // --------------------------------------

  // Returns the MetricLogger instance that should be used for logging Online
  // Wallet metrics.
  virtual const AutofillMetrics& GetMetricLogger() const = 0;

  // Returns the serialized fingerprint data to be sent to the Risk server.
  virtual std::string GetRiskData() const = 0;

  // Returns the cookie value used for authorization when making requests to
  // Wallet.
  virtual std::string GetWalletCookieValue() const = 0;

  // Whether or not shipping address is required by the delegate.
  virtual bool IsShippingAddressRequired() const = 0;

  // --------------------------------------------------------------------------
  // Callbacks called with responses from the Online Wallet backend.
  // --------------------------------------------------------------------------

  // Called when an AcceptLegalDocuments request finishes successfully.
  virtual void OnDidAcceptLegalDocuments() = 0;

  // Called when an AuthenticateInstrument request finishes successfully.
  virtual void OnDidAuthenticateInstrument(bool success) = 0;

  // Called when a GetFullWallet request finishes successfully. Ownership is
  // transferred to implementer of this interface.
  virtual void OnDidGetFullWallet(scoped_ptr<FullWallet> full_wallet) = 0;

  // Called when a GetWalletItems request finishes successfully. Ownership is
  // transferred to implementer of this interface.
  virtual void OnDidGetWalletItems(scoped_ptr<WalletItems> wallet_items) = 0;

  // Called when a SaveToWallet request finishes succesfully.
  // |instrument_id| and |address_id| can be used in subsequent
  // GetFullWallet calls. |required_actions| is populated if there was a
  // validation error with the data being saved. |form_field_errors| is
  // populated with the actual form fields that are failing validation.
  virtual void OnDidSaveToWallet(
      const std::string& instrument_id,
      const std::string& address_id,
      const std::vector<RequiredAction>& required_actions,
      const std::vector<FormFieldError>& form_field_errors) = 0;

  // Called when a request fails.
  virtual void OnWalletError(WalletClient::ErrorType error_type) = 0;

 protected:
  virtual ~WalletClientDelegate() {}
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_CLIENT_OBSERVER_H_
