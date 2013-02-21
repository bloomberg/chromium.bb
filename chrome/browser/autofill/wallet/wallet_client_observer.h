// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_WALLET_CLIENT_OBSERVER_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_WALLET_CLIENT_OBSERVER_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace autofill {
namespace wallet {

class FullWallet;
class WalletItems;

// WalletClientObserver is to be implemented any classes making calls with
// WalletClient. The appropriate callback method will be called on
// WalletClientObserver with the response from the Online Wallet backend.
class WalletClientObserver {
 public:
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
  // be used in subsequent GetFullWallet calls.
  virtual void OnDidSaveAddress(const std::string& address_id) = 0;

  // Called when a SaveInstrument request finishes sucessfully.
  // |instrument_id| can be used in subsequent GetFullWallet calls.
  virtual void OnDidSaveInstrument(const std::string& instrument_id) = 0;

  // Called when a SaveInstrumentAndAddress request finishes succesfully.
  // |instrument_id| and |address_id| can be used in subsequent
  // GetFullWallet calls.
  virtual void OnDidSaveInstrumentAndAddress(
      const std::string& instrument_id,
      const std::string& address_id) = 0;

  // Called when a SendAutocheckoutStatus request finishes successfully.
  virtual void OnDidSendAutocheckoutStatus() = 0;

  // Called when an UpdateInstrument request finishes successfully.
  virtual void OnDidUpdateInstrument(const std::string& instrument_id) = 0;

  // TODO(ahutter): This is going to need more arguments, probably an error
  // code and a message for the user.
  // Called when a request fails due to an Online Wallet error.
  virtual void OnWalletError() = 0;

  // Called when a request fails due to a malformed response.
  virtual void OnMalformedResponse() = 0;

  // Called when a request fails due to a network error.
  virtual void OnNetworkError(int response_code) = 0;

 protected:
  virtual ~WalletClientObserver() {}
};

}  // namespace wallet
}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_WALLET_CLIENT_OBSERVER_H_
