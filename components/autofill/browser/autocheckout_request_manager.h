// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_REQUEST_MANAGER_H_
#define COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_REQUEST_MANAGER_H_

#include "base/supports_user_data.h"
#include "components/autofill/browser/autofill_metrics.h"
#include "components/autofill/browser/wallet/wallet_client.h"
#include "components/autofill/browser/wallet/wallet_client_delegate.h"
#include "components/autofill/common/autocheckout_status.h"
#include "googleurl/src/gurl.h"

namespace content {
class BrowserContext;
}

namespace net {
class URLRequestContextGetter;
}

namespace autofill {

// AutocheckoutRequestManager's only responsiblity is to make sure any
// SendAutocheckoutStatus calls succeed regardless of any actions the user may
// make in the browser i.e. closing a tab, the requestAutocomplete dialog, etc.
// To that end, it is a piece of user data tied to the BrowserContext.
class AutocheckoutRequestManager : public base::SupportsUserData::Data,
                                   public wallet::WalletClientDelegate {
 public:
  virtual ~AutocheckoutRequestManager();

  // Creates a new AutocheckoutRequestManager and stores it as user data in
  // |browser_context| if one does not already exist.
  static void CreateForBrowserContext(
      content::BrowserContext* browser_context);

  // Retrieves the AutocheckoutRequestManager for |browser_context| if one
  // exists.
  static AutocheckoutRequestManager* FromBrowserContext(
      content::BrowserContext* browser_context);

  // Sends the |status| of an Autocheckout flow to Online Wallet using
  // |wallet_client_|.
  void SendAutocheckoutStatus(AutocheckoutStatus status,
                              const GURL& source_url,
                              const std::string& google_transaction_id);

  // wallet::WalletClientDelegate:
  virtual const AutofillMetrics& GetMetricLogger() const OVERRIDE;
  virtual DialogType GetDialogType() const OVERRIDE;
  virtual std::string GetRiskData() const OVERRIDE;
  virtual void OnDidAcceptLegalDocuments() OVERRIDE;
  virtual void OnDidAuthenticateInstrument(bool success) OVERRIDE;
  virtual void OnDidGetFullWallet(
      scoped_ptr<wallet::FullWallet> full_wallet) OVERRIDE;
  virtual void OnDidGetWalletItems(
      scoped_ptr<wallet::WalletItems> wallet_items) OVERRIDE;
  virtual void OnDidSaveAddress(
      const std::string& address_id,
      const std::vector<wallet::RequiredAction>& required_actions) OVERRIDE;
  virtual void OnDidSaveInstrument(
      const std::string& instrument_id,
      const std::vector<wallet::RequiredAction>& required_actions) OVERRIDE;
  virtual void OnDidSaveInstrumentAndAddress(
      const std::string& instrument_id,
      const std::string& address_id,
      const std::vector<wallet::RequiredAction>& required_actions) OVERRIDE;
  virtual void OnDidUpdateAddress(
      const std::string& address_id,
      const std::vector<wallet::RequiredAction>& required_actions) OVERRIDE;
  virtual void OnDidUpdateInstrument(
      const std::string& instrument_id,
      const std::vector<wallet::RequiredAction>& required_actions) OVERRIDE;
  virtual void OnWalletError(
      wallet::WalletClient::ErrorType error_type) OVERRIDE;
  virtual void OnMalformedResponse() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

 private:
  // |request_context_getter| is passed in to construct |wallet_client_|.
  AutocheckoutRequestManager(
      net::URLRequestContextGetter* request_context_getter);

  // Logs various UMA metrics.
  AutofillMetrics metric_logger_;

  // Makes requests to Online Wallet. The only request this class is configured
  // to make is SendAutocheckoutStatus.
  wallet::WalletClient wallet_client_;

  DISALLOW_COPY_AND_ASSIGN(AutocheckoutRequestManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_REQUEST_MANAGER_H_
