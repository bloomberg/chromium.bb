// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/payments/content/payment_details_validation.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
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
  // OnConnectionTerminated will be called when the Mojo pipe is closed. This
  // will happen as a result of many renderer-side events (both successful and
  // erroneous in nature).
  // TODO(crbug.com/683636): Investigate using
  // set_connection_error_with_reason_handler with Binding::CloseWithReason.
  binding_.set_connection_error_handler(base::Bind(
      &PaymentRequest::OnConnectionTerminated, base::Unretained(this)));
}

PaymentRequest::~PaymentRequest() {}

void PaymentRequest::Init(
    payments::mojom::PaymentRequestClientPtr client,
    std::vector<payments::mojom::PaymentMethodDataPtr> method_data,
    payments::mojom::PaymentDetailsPtr details,
    payments::mojom::PaymentOptionsPtr options) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string error;
  if (!payments::validatePaymentDetails(details, &error)) {
    LOG(ERROR) << error;
    OnConnectionTerminated();
    return;
  }
  client_ = std::move(client);
  spec_ = base::MakeUnique<PaymentRequestSpec>(
      std::move(options), std::move(details), std::move(method_data), this,
      delegate_->GetApplicationLocale());
  state_ = base::MakeUnique<PaymentRequestState>(
      spec_.get(), this, delegate_->GetApplicationLocale(),
      delegate_->GetPersonalDataManager());
}

void PaymentRequest::Show() {
  if (!client_.is_bound() || !binding_.is_bound()) {
    LOG(ERROR) << "Attempted Show(), but binding(s) missing.";
    OnConnectionTerminated();
    return;
  }
  delegate_->ShowDialog(this);
}

void PaymentRequest::Abort() {
  // The API user has decided to abort. We return a successful abort message to
  // the renderer, which closes the Mojo message pipe, which triggers
  // PaymentRequest::OnConnectionTerminated, which destroys this object.
  if (client_.is_bound())
    client_->OnAbort(true /* aborted_successfully */);
}

void PaymentRequest::Complete(payments::mojom::PaymentComplete result) {
  if (!client_.is_bound())
    return;

  // TODO(mathp): Validate |result|.

  // When the renderer closes the connection,
  // PaymentRequest::OnConnectionTerminated will be called.
  client_->OnComplete();
}

void PaymentRequest::CanMakePayment() {
  // TODO(crbug.com/704676): Implement a quota policy for this method.
  // PaymentRequest.canMakePayments() never returns false in incognito mode.
  client_->OnCanMakePayment(
      delegate_->IsIncognito() || state()->CanMakePayment()
          ? mojom::CanMakePaymentQueryResult::CAN_MAKE_PAYMENT
          : mojom::CanMakePaymentQueryResult::CANNOT_MAKE_PAYMENT);
}

void PaymentRequest::OnInvalidSpecProvided() {
  OnConnectionTerminated();
}

void PaymentRequest::OnPaymentResponseAvailable(
    mojom::PaymentResponsePtr response) {
  client_->OnPaymentResponse(std::move(response));
}

void PaymentRequest::UserCancelled() {
  // If |client_| is not bound, then the object is already being destroyed as
  // a result of a renderer event.
  if (!client_.is_bound())
    return;

  // This sends an error to the renderer, which informs the API user.
  client_->OnError(payments::mojom::PaymentErrorReason::USER_CANCEL);

  // We close all bindings and ask to be destroyed.
  client_.reset();
  binding_.Close();
  manager_->DestroyRequest(this);
}

void PaymentRequest::OnConnectionTerminated() {
  // We are here because of a browser-side error, or likely as a result of the
  // connection_error_handler on |binding_|, which can mean that the renderer
  // has decided to close the pipe for various reasons (see all uses of
  // PaymentRequest::clearResolversAndCloseMojoConnection() in Blink). We close
  // the binding and the dialog, and ask to be deleted.
  client_.reset();
  binding_.Close();
  delegate_->CloseDialog();
  manager_->DestroyRequest(this);
}

void PaymentRequest::Pay() {
  state_->GeneratePaymentResponse();
}

}  // namespace payments
