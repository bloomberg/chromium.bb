// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/payment_request_factory.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/payments/chrome_payment_request_delegate.h"
#include "components/payments/content/payment_request_delegate.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "content/public/browser/web_contents.h"

namespace payments {

void CreatePaymentRequestForWebContents(
    content::WebContents* web_contents,
    mojo::InterfaceRequest<payments::mojom::PaymentRequest> request) {
  DCHECK(web_contents);
  PaymentRequestWebContentsManager::GetOrCreateForWebContents(web_contents)
      ->CreatePaymentRequest(web_contents,
                             base::MakeUnique<ChromePaymentRequestDelegate>(
                                web_contents),
                             std::move(request));
}

}  // namespace payments
