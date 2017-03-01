// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/chrome_payment_request_delegate.h"

#include "base/macros.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "components/payments/content/payment_request_dialog.h"
#include "content/public/browser/web_contents.h"

namespace payments {

ChromePaymentRequestDelegate::ChromePaymentRequestDelegate(
    content::WebContents* web_contents)
    : dialog_(nullptr), web_contents_(web_contents) {}

void ChromePaymentRequestDelegate::ShowDialog(PaymentRequest* request) {
  DCHECK_EQ(nullptr, dialog_);
  dialog_ = chrome::CreatePaymentRequestDialog(request);
  dialog_->ShowDialog();
}

void ChromePaymentRequestDelegate::CloseDialog() {
  if (dialog_) {
    dialog_->CloseDialog();
    dialog_ = nullptr;
  }
}

autofill::PersonalDataManager*
ChromePaymentRequestDelegate::GetPersonalDataManager() {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

const std::string& ChromePaymentRequestDelegate::GetApplicationLocale() const {
  return g_browser_process->GetApplicationLocale();
}

}  // namespace payments
