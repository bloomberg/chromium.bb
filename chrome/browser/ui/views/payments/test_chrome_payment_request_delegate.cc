// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/test_chrome_payment_request_delegate.h"

#include <utility>

#include "content/public/browser/web_contents.h"

namespace payments {

TestChromePaymentRequestDelegate::TestChromePaymentRequestDelegate(
    content::WebContents* web_contents,
    PaymentRequestDialogView::ObserverForTest* observer,
    bool is_incognito,
    bool is_valid_ssl)
    : ChromePaymentRequestDelegate(web_contents),
      region_data_loader_(nullptr),
      observer_(observer),
      is_incognito_(is_incognito),
      is_valid_ssl_(is_valid_ssl) {}

void TestChromePaymentRequestDelegate::ShowDialog(PaymentRequest* request) {
  PaymentRequestDialogView* dialog_view =
      new PaymentRequestDialogView(request, observer_);
  dialog_view->ShowDialog();
  dialog_ = std::move(dialog_view);
}

bool TestChromePaymentRequestDelegate::IsIncognito() const {
  return is_incognito_;
}

bool TestChromePaymentRequestDelegate::IsSslCertificateValid() {
  return is_valid_ssl_;
}

autofill::RegionDataLoader*
TestChromePaymentRequestDelegate::GetRegionDataLoader() {
  if (region_data_loader_)
    return region_data_loader_;
  return ChromePaymentRequestDelegate::GetRegionDataLoader();
}
}  // namespace payments
