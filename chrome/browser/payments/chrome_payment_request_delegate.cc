// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/chrome_payment_request_delegate.h"

#include "chrome/browser/ui/browser_dialogs.h"

namespace payments {

void ChromePaymentRequestDelegate::ShowPaymentRequestDialog(
    PaymentRequest* request) {
  chrome::ShowPaymentRequestDialog(request);
}

}  // namespace payments
