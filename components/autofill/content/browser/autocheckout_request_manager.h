// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOCHECKOUT_REQUEST_MANAGER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOCHECKOUT_REQUEST_MANAGER_H_

#include "base/supports_user_data.h"
#include "components/autofill/content/browser/autocheckout_statistic.h"
#include "components/autofill/content/browser/wallet/wallet_client.h"
#include "components/autofill/content/browser/wallet/wallet_client_delegate.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/common/autocheckout_status.h"
#include "url/gurl.h"

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
  void SendAutocheckoutStatus(
      AutocheckoutStatus status,
      const GURL& source_url,
      const std::vector<AutocheckoutStatistic>& latency_statistics,
      const std::string& google_transaction_id);

  // wallet::WalletClientDelegate:
  virtual const AutofillMetrics& GetMetricLogger() const OVERRIDE;
  virtual DialogType GetDialogType() const OVERRIDE;
  virtual std::string GetRiskData() const OVERRIDE;
  virtual std::string GetWalletCookieValue() const OVERRIDE;
  virtual bool IsShippingAddressRequired() const OVERRIDE;
  virtual void OnDidAcceptLegalDocuments() OVERRIDE;
  virtual void OnDidAuthenticateInstrument(bool success) OVERRIDE;
  virtual void OnDidGetFullWallet(
      scoped_ptr<wallet::FullWallet> full_wallet) OVERRIDE;
  virtual void OnDidGetWalletItems(
      scoped_ptr<wallet::WalletItems> wallet_items) OVERRIDE;
  virtual void OnDidSaveToWallet(
      const std::string& instrument_id,
      const std::string& address_id,
      const std::vector<wallet::RequiredAction>& required_actions,
      const std::vector<wallet::FormFieldError>& form_field_errors) OVERRIDE;
  virtual void OnWalletError(
      wallet::WalletClient::ErrorType error_type) OVERRIDE;

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

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOCHECKOUT_REQUEST_MANAGER_H_
