// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/payment_request_impl.h"

#include <map>

#include "base/lazy_instance.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "components/payments/payment_details_validation.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace {

class PaymentRequestFactory {
 public:
  bool AssignPaymentRequest(
      content::WebContents* web_contents,
      mojo::InterfaceRequest<payments::mojom::PaymentRequest> request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (map_.find(web_contents) == map_.end()) {
      scoped_refptr<payments::PaymentRequestImpl> p(
          new payments::PaymentRequestImpl(web_contents, std::move(request)));
      map_[web_contents] = p;
      return true;
    }
    return false;
  }

  void UnassignPaymentRequest(content::WebContents* web_contents) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    map_.erase(web_contents);
  }

 private:
  std::map<content::WebContents*, scoped_refptr<payments::PaymentRequestImpl>>
      map_;
};

base::LazyInstance<PaymentRequestFactory> payment_request_factory;

}  // namespace

namespace payments {

PaymentRequestImpl::PaymentRequestImpl(
    content::WebContents* web_contents,
    mojo::InterfaceRequest<payments::mojom::PaymentRequest> request)
    : web_contents_(web_contents),
      binding_(this, std::move(request)) {
  binding_.set_connection_error_handler(
      base::Bind(&PaymentRequestImpl::OnError, this));
}

PaymentRequestImpl::~PaymentRequestImpl() {}

void PaymentRequestImpl::Init(
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

void PaymentRequestImpl::Show() {
  chrome::ShowPaymentRequestDialog(this);
}

void PaymentRequestImpl::Cancel() {
  client_->OnError(payments::mojom::PaymentErrorReason::USER_CANCEL);
}

void PaymentRequestImpl::OnError() {
  binding_.Close();
  // TODO(krb): Close the dialog here, but avoid double-free
  payment_request_factory.Get().UnassignPaymentRequest(web_contents_);
}

}  // namespace payments

void CreatePaymentRequestHandler(
    content::WebContents* web_contents,
    mojo::InterfaceRequest<payments::mojom::PaymentRequest> request) {
  payment_request_factory.Get().AssignPaymentRequest(web_contents,
                                                     std::move(request));
}
