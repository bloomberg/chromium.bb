// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/chrome_payment_request_delegate.h"

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "content/public/browser/web_contents.h"

namespace payments {

ChromePaymentRequestDelegate::ChromePaymentRequestDelegate(
    content::WebContents* web_contents)
  : web_contents_(web_contents) {}

void ChromePaymentRequestDelegate::ShowPaymentRequestDialog(
    PaymentRequest* request) {
  chrome::ShowPaymentRequestDialog(request);
}

autofill::PersonalDataManager*
ChromePaymentRequestDelegate::GetPersonalDataManager() {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

}  // namespace payments
