// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/autocheckout_request_manager.h"

#include "components/autofill/core/browser/autofill_manager_delegate.h"
#include "content/public/browser/browser_context.h"

namespace {

const char kAutocheckoutRequestManagerKey[] =
    "browser_context_autocheckout_request_manager";

}  // namespace

namespace autofill {

AutocheckoutRequestManager::~AutocheckoutRequestManager() {}

// static
void AutocheckoutRequestManager::CreateForBrowserContext(
    content::BrowserContext* browser_context) {
  if (FromBrowserContext(browser_context))
    return;

  browser_context->SetUserData(
      kAutocheckoutRequestManagerKey,
      new AutocheckoutRequestManager(browser_context->GetRequestContext()));
}

// static
AutocheckoutRequestManager* AutocheckoutRequestManager::FromBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<AutocheckoutRequestManager*>(
      browser_context->GetUserData(kAutocheckoutRequestManagerKey));
}

void AutocheckoutRequestManager::SendAutocheckoutStatus(
    AutocheckoutStatus status,
    const GURL& source_url,
    const std::vector<AutocheckoutStatistic>& latency_statistics,
    const std::string& google_transaction_id) {
  wallet_client_.SendAutocheckoutStatus(status,
                                        source_url,
                                        latency_statistics,
                                        google_transaction_id);
}


const AutofillMetrics& AutocheckoutRequestManager::GetMetricLogger() const {
  return metric_logger_;
}

DialogType AutocheckoutRequestManager::GetDialogType() const {
  return DIALOG_TYPE_AUTOCHECKOUT;
}

std::string AutocheckoutRequestManager::GetRiskData() const {
  NOTREACHED();
  return std::string();
}

std::string AutocheckoutRequestManager::GetWalletCookieValue() const {
  return std::string();
}

bool AutocheckoutRequestManager::IsShippingAddressRequired() const {
  NOTREACHED();
  return true;
}

void AutocheckoutRequestManager::OnDidAcceptLegalDocuments() {
  NOTREACHED();
}

void AutocheckoutRequestManager::OnDidAuthenticateInstrument(bool success) {
  NOTREACHED();
}

void AutocheckoutRequestManager::OnDidGetFullWallet(
    scoped_ptr<wallet::FullWallet> full_wallet) {
  NOTREACHED();
}

void AutocheckoutRequestManager::OnDidGetWalletItems(
    scoped_ptr<wallet::WalletItems> wallet_items) {
  NOTREACHED();
}


void AutocheckoutRequestManager::OnDidSaveToWallet(
    const std::string& instrument_id,
    const std::string& address_id,
    const std::vector<wallet::RequiredAction>& required_actions,
    const std::vector<wallet::FormFieldError>& form_field_errors) {
  NOTREACHED();
}

void AutocheckoutRequestManager::OnWalletError(
    wallet::WalletClient::ErrorType error_type) {
  // Nothing to be done. |error_type| is logged by |metric_logger_|.
}

AutocheckoutRequestManager::AutocheckoutRequestManager(
    net::URLRequestContextGetter* request_context_getter)
    : wallet_client_(request_context_getter, this) {
}

}  // namespace autofill
