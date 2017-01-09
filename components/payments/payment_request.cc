// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/payment_request.h"

#include "components/payments/payment_details_validation.h"
#include "components/payments/payment_request_delegate.h"
#include "components/payments/payment_request_web_contents_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace payments {

PaymentRequest::PaymentRequest(
    content::WebContents* web_contents,
    std::unique_ptr<PaymentRequestDelegate> delegate,
    PaymentRequestWebContentsManager* manager,
    mojo::InterfaceRequest<payments::mojom::PaymentRequest> request)
    : web_contents_(web_contents),
      delegate_(std::move(delegate)),
      manager_(manager),
      binding_(this, std::move(request)) {
  binding_.set_connection_error_handler(
      base::Bind(&PaymentRequest::OnError, base::Unretained(this)));
}

PaymentRequest::~PaymentRequest() {}

void PaymentRequest::Init(
    payments::mojom::PaymentRequestClientPtr client,
    std::vector<payments::mojom::PaymentMethodDataPtr> methodData,
    payments::mojom::PaymentDetailsPtr details,
    payments::mojom::PaymentOptionsPtr options) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string error;
  if (!payments::validatePaymentDetails(details, &error)) {
    LOG(ERROR) << error;
    OnError();
    return;
  }
  client_ = std::move(client);
  details_ = std::move(details);
}

void PaymentRequest::Show() {
  delegate_->ShowPaymentRequestDialog(this);
}

void PaymentRequest::Cancel() {
  client_->OnError(payments::mojom::PaymentErrorReason::USER_CANCEL);
}

void PaymentRequest::OnError() {
  binding_.Close();
  manager_->DestroyRequest(this);
}

}  // namespace payments
