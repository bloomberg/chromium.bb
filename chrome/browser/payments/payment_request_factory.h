// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_FACTORY_H_
#define CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_FACTORY_H_

#include "components/payments/mojom/payment_request.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace content {
class WebContents;
}

namespace payments {

// Will create a PaymentRequest attached to |web_contents|, based on the
// contents of |request|. This is called everytime a new Mojo PaymentRequest is
// created.
void CreatePaymentRequestForWebContents(
    content::WebContents* web_contents,
    const service_manager::BindSourceInfo& source_info,
    payments::mojom::PaymentRequestRequest request);

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_FACTORY_H_
