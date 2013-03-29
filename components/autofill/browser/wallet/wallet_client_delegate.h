// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_CLIENT_OBSERVER_H_
#define COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_CLIENT_OBSERVER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "components/autofill/browser/autofill_manager_delegate.h"
#include "components/autofill/browser/wallet/wallet_client.h"

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

  // Returns the dialog type that the delegate corresponds to.
  virtual DialogType GetDialogType() const = 0;

  // Returns the serialized fingerprint data to be sent to the Risk server.
  virtual std::string GetRiskData() const = 0;

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

  // Called when a SaveAddress request finishes successfully. |address_id| can
  // be used in subsequent GetFullWallet calls. |required_actions| is populated
  // if there was a validation error with the data being saved.
  virtual void OnDidSaveAddress(
      const std::string& address_id,
      const std::vector<RequiredAction>& required_actions) = 0;

  // Called when a SaveInstrument request finishes sucessfully. |instrument_id|
  // can be used in subsequent GetFullWallet calls. |required_actions| is
  // populated if there was a validation error with the data being saved.
  virtual void OnDidSaveInstrument(
      const std::string& instrument_id,
      const std::vector<RequiredAction>& required_actions) = 0;

  // Called when a SaveInstrumentAndAddress request finishes succesfully.
  // |instrument_id| and |address_id| can be used in subsequent
  // GetFullWallet calls. |required_actions| is populated if there was a
  // validation error with the data being saved.
  virtual void OnDidSaveInstrumentAndAddress(
      const std::string& instrument_id,
      const std::string& address_id,
      const std::vector<RequiredAction>& required_actions) = 0;

  // Called when an UpdateAddress request finishes successfully.
  // |required_actions| is populated if there was a validation error with the
  // data being saved.
  virtual void OnDidUpdateAddress(
      const std::string& address_id,
      const std::vector<RequiredAction>& required_actions) = 0;

  // Called when an UpdateInstrument request finishes successfully.
  // |required_actions| is populated if there was a validation error with the
  // data being saved.
  virtual void OnDidUpdateInstrument(
      const std::string& instrument_id,
      const std::vector<RequiredAction>& required_actions) = 0;

  // Called when a request fails due to an Online Wallet error.
  virtual void OnWalletError(WalletClient::ErrorType error_type) = 0;

  // Called when a request fails due to a malformed response.
  virtual void OnMalformedResponse() = 0;

  // Called when a request fails due to a network error.
  virtual void OnNetworkError(int response_code) = 0;

 protected:
  virtual ~WalletClientDelegate() {}
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_CLIENT_OBSERVER_H_
