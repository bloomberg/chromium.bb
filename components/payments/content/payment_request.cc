// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "components/payments/content/can_make_payment_query_factory.h"
#include "components/payments/content/origin_security_checker.h"
#include "components/payments/content/payment_details_validation.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "components/payments/core/can_make_payment_query.h"
#include "components/payments/core/payment_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace payments {

PaymentRequest::PaymentRequest(
    content::RenderFrameHost* render_frame_host,
    content::WebContents* web_contents,
    std::unique_ptr<PaymentRequestDelegate> delegate,
    PaymentRequestWebContentsManager* manager,
    mojo::InterfaceRequest<mojom::PaymentRequest> request,
    ObserverForTest* observer_for_testing)
    : web_contents_(web_contents),
      delegate_(std::move(delegate)),
      manager_(manager),
      binding_(this, std::move(request)),
      frame_origin_(GURL(render_frame_host->GetLastCommittedURL()).GetOrigin()),
      observer_for_testing_(observer_for_testing),
      journey_logger_(delegate_->IsIncognito(),
                      web_contents_->GetLastCommittedURL(),
                      delegate_->GetUkmRecorder()) {
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

  const GURL last_committed_url = delegate_->GetLastCommittedURL();
  if (!OriginSecurityChecker::IsOriginSecure(last_committed_url)) {
    LOG(ERROR) << "Not in a secure origin";
    OnConnectionTerminated();
    return;
  }

  bool allowed_origin =
      OriginSecurityChecker::IsSchemeCryptographic(last_committed_url) ||
      OriginSecurityChecker::IsOriginLocalhostOrFile(last_committed_url);
  if (!allowed_origin) {
    LOG(ERROR) << "Only localhost, file://, and cryptographic scheme origins "
                  "allowed";
  }

  bool invalid_ssl =
      OriginSecurityChecker::IsSchemeCryptographic(last_committed_url) &&
      !delegate_->IsSslCertificateValid();
  if (invalid_ssl)
    LOG(ERROR) << "SSL certificate is not valid";

  if (!allowed_origin || invalid_ssl) {
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

  // A tab can display only one PaymentRequest UI at a time.
  if (!manager_->CanShow(this)) {
    LOG(ERROR) << "A PaymentRequest UI is already showing";
    journey_logger_.SetNotShown(
        JourneyLogger::NOT_SHOWN_REASON_CONCURRENT_REQUESTS);
    has_recorded_completion_ = true;
    client_->OnError(mojom::PaymentErrorReason::USER_CANCEL);
    OnConnectionTerminated();
    return;
  }

  if (!state_->AreRequestedMethodsSupported()) {
    journey_logger_.SetNotShown(
        JourneyLogger::NOT_SHOWN_REASON_NO_SUPPORTED_PAYMENT_METHOD);
    has_recorded_completion_ = true;
    client_->OnError(mojom::PaymentErrorReason::NOT_SUPPORTED);
    if (observer_for_testing_)
      observer_for_testing_->OnNotSupportedError();
    OnConnectionTerminated();
    return;
  }

  journey_logger_.SetShowCalled();
  journey_logger_.SetRequestedInformation(
      spec_->request_shipping(), spec_->request_payer_email(),
      spec_->request_payer_phone(), spec_->request_payer_name());

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
  // The API user has decided to abort. If a successful abort message is
  // returned to the renderer, the Mojo message pipe is closed, which triggers
  // PaymentRequest::OnConnectionTerminated, which destroys this object.
  // Otherwise, the abort promise is rejected and the pipe is not closed.
  // The abort is only successful if the payment app wasn't yet invoked.
  // TODO(crbug.com/716546): Add a merchant abort metric

  bool accepting_abort = !state_->IsPaymentAppInvoked();
  if (accepting_abort)
    RecordFirstAbortReason(JourneyLogger::ABORT_REASON_ABORTED_BY_MERCHANT);

  if (client_.is_bound())
    client_->OnAbort(accepting_abort);

  if (observer_for_testing_)
    observer_for_testing_->OnAbortCalled();
}

void PaymentRequest::Complete(mojom::PaymentComplete result) {
  if (!client_.is_bound())
    return;

  // Failed transactions show an error. Successful and unknown-state
  // transactions don't show an error.
  if (result == mojom::PaymentComplete::FAIL) {
    delegate_->ShowErrorMessage();
  } else {
    DCHECK(!has_recorded_completion_);
    journey_logger_.SetCompleted();
    has_recorded_completion_ = true;

    delegate_->GetPrefService()->SetBoolean(kPaymentsFirstTransactionCompleted,
                                            true);
    // When the renderer closes the connection,
    // PaymentRequest::OnConnectionTerminated will be called.
    client_->OnComplete();
    state_->RecordUseStats();
  }
}

void PaymentRequest::CanMakePayment() {
  bool can_make_payment = state()->CanMakePayment();
  if (delegate_->IsIncognito()) {
    client_->OnCanMakePayment(
        mojom::CanMakePaymentQueryResult::CAN_MAKE_PAYMENT);
    journey_logger_.SetCanMakePaymentValue(true);
  } else if (CanMakePaymentQueryFactory::GetInstance()
                 ->GetForContext(web_contents_->GetBrowserContext())
                 ->CanQuery(frame_origin_, spec()->stringified_method_data())) {
    client_->OnCanMakePayment(
        can_make_payment
            ? mojom::CanMakePaymentQueryResult::CAN_MAKE_PAYMENT
            : mojom::CanMakePaymentQueryResult::CANNOT_MAKE_PAYMENT);
    journey_logger_.SetCanMakePaymentValue(can_make_payment);
  } else if (OriginSecurityChecker::IsOriginLocalhostOrFile(frame_origin_)) {
    client_->OnCanMakePayment(
        can_make_payment
            ? mojom::CanMakePaymentQueryResult::WARNING_CAN_MAKE_PAYMENT
            : mojom::CanMakePaymentQueryResult::WARNING_CANNOT_MAKE_PAYMENT);
    journey_logger_.SetCanMakePaymentValue(can_make_payment);
  } else {
    client_->OnCanMakePayment(
        mojom::CanMakePaymentQueryResult::QUERY_QUOTA_EXCEEDED);
  }

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

  RecordFirstAbortReason(JourneyLogger::ABORT_REASON_ABORTED_BY_USER);

  // This sends an error to the renderer, which informs the API user.
  client_->OnError(mojom::PaymentErrorReason::USER_CANCEL);

  // We close all bindings and ask to be destroyed.
  client_.reset();
  binding_.Close();
  if (observer_for_testing_)
    observer_for_testing_->OnConnectionTerminated();
  manager_->DestroyRequest(this);
}

void PaymentRequest::DidStartNavigation(bool is_user_initiated) {
  RecordFirstAbortReason(is_user_initiated
                             ? JourneyLogger::ABORT_REASON_USER_NAVIGATION
                             : JourneyLogger::ABORT_REASON_MERCHANT_NAVIGATION);
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
  if (observer_for_testing_)
    observer_for_testing_->OnConnectionTerminated();

  RecordFirstAbortReason(JourneyLogger::ABORT_REASON_MOJO_CONNECTION_ERROR);
  manager_->DestroyRequest(this);
}

void PaymentRequest::Pay() {
  journey_logger_.SetSelectedPaymentMethod(
      JourneyLogger::SELECTED_PAYMENT_METHOD_CREDIT_CARD);
  state_->GeneratePaymentResponse();
}

void PaymentRequest::RecordFirstAbortReason(
    JourneyLogger::AbortReason abort_reason) {
  if (!has_recorded_completion_) {
    has_recorded_completion_ = true;
    journey_logger_.SetAborted(abort_reason);
  }
}

}  // namespace payments
