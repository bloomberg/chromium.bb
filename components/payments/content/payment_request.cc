// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "components/payments/content/origin_security_checker.h"
#include "components/payments/content/payment_details_validation.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace payments {

PaymentRequest::PaymentRequest(
    content::WebContents* web_contents,
    std::unique_ptr<PaymentRequestDelegate> delegate,
    PaymentRequestWebContentsManager* manager,
    mojo::InterfaceRequest<mojom::PaymentRequest> request,
    ObserverForTest* observer_for_testing)
    : web_contents_(web_contents),
      delegate_(std::move(delegate)),
      manager_(manager),
      binding_(this, std::move(request)),
      observer_for_testing_(observer_for_testing) {
  // OnConnectionTerminated will be called when the Mojo pipe is closed. This
  // will happen as a result of many renderer-side events (both successful and
  // erroneous in nature).
  // TODO(crbug.com/683636): Investigate using
  // set_connection_error_with_reason_handler with Binding::CloseWithReason.
  binding_.set_connection_error_handler(base::Bind(
      &PaymentRequest::OnConnectionTerminated, base::Unretained(this)));
}

PaymentRequest::~PaymentRequest() {}

void PaymentRequest::Init(mojom::PaymentRequestClientPtr client,
                          std::vector<mojom::PaymentMethodDataPtr> method_data,
                          mojom::PaymentDetailsPtr details,
                          mojom::PaymentOptionsPtr options) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  client_ = std::move(client);

  if (!OriginSecurityChecker::IsOriginSecure(
          delegate_->GetLastCommittedURL())) {
    LOG(ERROR) << "Not in a secure origin";
    OnConnectionTerminated();
    return;
  }

  if (OriginSecurityChecker::IsSchemeCryptographic(
          delegate_->GetLastCommittedURL()) &&
      !delegate_->IsSslCertificateValid()) {
    LOG(ERROR) << "SSL certificate is not valid";
    // Don't show UI. Resolve .canMakepayment() with "false". Reject .show()
    // with "NotSupportedError".
    spec_ = base::MakeUnique<PaymentRequestSpec>(
        mojom::PaymentOptions::New(), mojom::PaymentDetails::New(),
        std::vector<mojom::PaymentMethodDataPtr>(), this,
        delegate_->GetApplicationLocale());
    state_ = base::MakeUnique<PaymentRequestState>(
        spec_.get(), this, delegate_->GetApplicationLocale(),
        delegate_->GetPersonalDataManager(), delegate_.get());
    return;
  }

  std::string error;
  if (!validatePaymentDetails(details, &error)) {
    LOG(ERROR) << error;
    OnConnectionTerminated();
    return;
  }

  if (!details->total) {
    LOG(ERROR) << "Missing total";
    OnConnectionTerminated();
    return;
  }

  spec_ = base::MakeUnique<PaymentRequestSpec>(
      std::move(options), std::move(details), std::move(method_data), this,
      delegate_->GetApplicationLocale());
  state_ = base::MakeUnique<PaymentRequestState>(
      spec_.get(), this, delegate_->GetApplicationLocale(),
      delegate_->GetPersonalDataManager(), delegate_.get());
}

void PaymentRequest::Show() {
  if (!client_.is_bound() || !binding_.is_bound()) {
    LOG(ERROR) << "Attempted Show(), but binding(s) missing.";
    OnConnectionTerminated();
    return;
  }

  if (!state_->AreRequestedMethodsSupported()) {
    client_->OnError(mojom::PaymentErrorReason::NOT_SUPPORTED);
    if (observer_for_testing_)
      observer_for_testing_->OnNotSupportedError();
    OnConnectionTerminated();
    return;
  }

  delegate_->ShowDialog(this);
}

void PaymentRequest::UpdateWith(mojom::PaymentDetailsPtr details) {
  std::string error;
  if (!validatePaymentDetails(details, &error)) {
    LOG(ERROR) << error;
    OnConnectionTerminated();
    return;
  }
  spec_->UpdateWith(std::move(details));
}

void PaymentRequest::Abort() {
  // The API user has decided to abort. We return a successful abort message to
  // the renderer, which closes the Mojo message pipe, which triggers
  // PaymentRequest::OnConnectionTerminated, which destroys this object.
  if (client_.is_bound())
    client_->OnAbort(true /* aborted_successfully */);
}

void PaymentRequest::Complete(mojom::PaymentComplete result) {
  if (!client_.is_bound())
    return;

  if (result != mojom::PaymentComplete::SUCCESS) {
    delegate_->ShowErrorMessage();
  } else {
    // When the renderer closes the connection,
    // PaymentRequest::OnConnectionTerminated will be called.
    client_->OnComplete();
  }
}

void PaymentRequest::CanMakePayment() {
  // TODO(crbug.com/704676): Implement a quota policy for this method.
  // PaymentRequest.canMakePayments() never returns false in incognito mode.
  client_->OnCanMakePayment(
      delegate_->IsIncognito() || state()->CanMakePayment()
          ? mojom::CanMakePaymentQueryResult::CAN_MAKE_PAYMENT
          : mojom::CanMakePaymentQueryResult::CANNOT_MAKE_PAYMENT);
  if (observer_for_testing_)
    observer_for_testing_->OnCanMakePaymentCalled();
}

void PaymentRequest::OnPaymentResponseAvailable(
    mojom::PaymentResponsePtr response) {
  client_->OnPaymentResponse(std::move(response));
}

void PaymentRequest::OnShippingOptionIdSelected(
    std::string shipping_option_id) {
  client_->OnShippingOptionChange(shipping_option_id);
}

void PaymentRequest::OnShippingAddressSelected(
    mojom::PaymentAddressPtr address) {
  client_->OnShippingAddressChange(std::move(address));
}

void PaymentRequest::UserCancelled() {
  // If |client_| is not bound, then the object is already being destroyed as
  // a result of a renderer event.
  if (!client_.is_bound())
    return;

  // This sends an error to the renderer, which informs the API user.
  client_->OnError(mojom::PaymentErrorReason::USER_CANCEL);

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
