// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/autocheckout_request_manager.h"

#include "components/autofill/browser/autofill_manager_delegate.h"
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
    const std::string& google_transaction_id) {
  wallet_client_.SendAutocheckoutStatus(status,
                                        source_url,
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

void AutocheckoutRequestManager::OnDidSaveAddress(
    const std::string& address_id,
    const std::vector<wallet::RequiredAction>& required_actions) {
  NOTREACHED();
}

void AutocheckoutRequestManager::OnDidSaveInstrument(
    const std::string& instrument_id,
    const std::vector<wallet::RequiredAction>& required_actions) {
  NOTREACHED();
}

void AutocheckoutRequestManager::OnDidSaveInstrumentAndAddress(
    const std::string& instrument_id,
    const std::string& address_id,
    const std::vector<wallet::RequiredAction>& required_actions) {
  NOTREACHED();
}

void AutocheckoutRequestManager::OnDidUpdateAddress(
    const std::string& address_id,
    const std::vector<wallet::RequiredAction>& required_actions) {
  NOTREACHED();
}

void AutocheckoutRequestManager::OnDidUpdateInstrument(
    const std::string& instrument_id,
    const std::vector<wallet::RequiredAction>& required_actions) {
  NOTREACHED();
}

void AutocheckoutRequestManager::OnWalletError(
    wallet::WalletClient::ErrorType error_type) {
  // Nothing to be done. |error_type| is logged by |metric_logger_|.
}

void AutocheckoutRequestManager::OnMalformedResponse() {
  // Nothing to be done.
}

void AutocheckoutRequestManager::OnNetworkError(int response_code) {
  // Nothin to be done. |response_code| is logged by |metric_logger_|.
}

AutocheckoutRequestManager::AutocheckoutRequestManager(
    net::URLRequestContextGetter* request_context_getter)
    : wallet_client_(request_context_getter, this) {
}

}  // namespace autofill
