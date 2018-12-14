// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/stl_util.h"
#include "components/payments/content/can_make_payment_query_factory.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/origin_security_checker.h"
#include "components/payments/content/payment_request_converter.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "components/payments/core/can_make_payment_query.h"
#include "components/payments/core/features.h"
#include "components/payments/core/payment_details.h"
#include "components/payments/core/payment_details_validation.h"
#include "components/payments/core/payment_instrument.h"
#include "components/payments/core/payment_prefs.h"
#include "components/payments/core/payments_validators.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

namespace payments {

PaymentRequest::PaymentRequest(
    content::RenderFrameHost* render_frame_host,
    content::WebContents* web_contents,
    std::unique_ptr<ContentPaymentRequestDelegate> delegate,
    PaymentRequestWebContentsManager* manager,
    PaymentRequestDisplayManager* display_manager,
    mojo::InterfaceRequest<mojom::PaymentRequest> request,
    ObserverForTest* observer_for_testing)
    : web_contents_(web_contents),
      log_(web_contents_),
      delegate_(std::move(delegate)),
      manager_(manager),
      display_manager_(display_manager),
      display_handle_(nullptr),
      binding_(this, std::move(request)),
      top_level_origin_(url_formatter::FormatUrlForSecurityDisplay(
          web_contents_->GetLastCommittedURL())),
      frame_origin_(url_formatter::FormatUrlForSecurityDisplay(
          render_frame_host->GetLastCommittedURL())),
      observer_for_testing_(observer_for_testing),
      journey_logger_(delegate_->IsIncognito(),
                      ukm::GetSourceIdForWebContentsDocument(web_contents)),
      weak_ptr_factory_(this) {
  // OnConnectionTerminated will be called when the Mojo pipe is closed. This
  // will happen as a result of many renderer-side events (both successful and
  // erroneous in nature).
  // TODO(crbug.com/683636): Investigate using
  // set_connection_error_with_reason_handler with Binding::CloseWithReason.
  binding_.set_connection_error_handler(base::BindOnce(
      &PaymentRequest::OnConnectionTerminated, weak_ptr_factory_.GetWeakPtr()));
}

PaymentRequest::~PaymentRequest() {}

void PaymentRequest::Init(mojom::PaymentRequestClientPtr client,
                          std::vector<mojom::PaymentMethodDataPtr> method_data,
                          mojom::PaymentDetailsPtr details,
                          mojom::PaymentOptionsPtr options) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_initialized_) {
    log_.Error("Attempted initialization twice");
    OnConnectionTerminated();
    return;
  }

  is_initialized_ = true;
  client_ = std::move(client);

  const GURL last_committed_url = delegate_->GetLastCommittedURL();
  if (!OriginSecurityChecker::IsOriginSecure(last_committed_url)) {
    log_.Error("Not in a secure origin");
    OnConnectionTerminated();
    return;
  }

  bool allowed_origin =
      OriginSecurityChecker::IsSchemeCryptographic(last_committed_url) ||
      OriginSecurityChecker::IsOriginLocalhostOrFile(last_committed_url);
  if (!allowed_origin) {
    log_.Error(
        "Only localhost, file://, and cryptographic scheme origins allowed");
  }

  bool invalid_ssl =
      OriginSecurityChecker::IsSchemeCryptographic(last_committed_url) &&
      !delegate_->IsSslCertificateValid();
  if (invalid_ssl) {
    log_.Error("SSL certificate is not valid.");
  }

  if (!allowed_origin || invalid_ssl) {
    // Intentionally don't set |spec_| and |state_|, so the UI is never shown.
    log_.Error(
        "No UI will be shown. CanMakePayment will always return false. "
        "Show will be rejected with NotSupportedError.");
    return;
  }

  std::string error;
  if (!ValidatePaymentDetails(ConvertPaymentDetails(details), &error)) {
    log_.Error(error);
    OnConnectionTerminated();
    return;
  }

  if (!details->total) {
    log_.Error("Missing total");
    OnConnectionTerminated();
    return;
  }

  spec_ = std::make_unique<PaymentRequestSpec>(
      std::move(options), std::move(details), std::move(method_data), this,
      delegate_->GetApplicationLocale());
  state_ = std::make_unique<PaymentRequestState>(
      web_contents_, top_level_origin_, frame_origin_, spec_.get(), this,
      delegate_->GetApplicationLocale(), delegate_->GetPersonalDataManager(),
      delegate_.get(), &journey_logger_);

  journey_logger_.SetRequestedInformation(
      spec_->request_shipping(), spec_->request_payer_email(),
      spec_->request_payer_phone(), spec_->request_payer_name());

  // Log metrics around which payment methods are requested by the merchant.
  GURL google_pay_url(kGooglePayMethodName);
  GURL android_pay_url(kAndroidPayMethodName);
  // Looking for payment methods that are NOT google-related payment methods.
  auto non_google_it =
      std::find_if(spec_->url_payment_method_identifiers().begin(),
                   spec_->url_payment_method_identifiers().end(),
                   [google_pay_url, android_pay_url](const GURL& url) {
                     return url != google_pay_url && url != android_pay_url;
                   });
  journey_logger_.SetRequestedPaymentMethodTypes(
      /*requested_basic_card=*/!spec_->supported_card_networks().empty(),
      /*requested_method_google=*/
      base::ContainsValue(spec_->url_payment_method_identifiers(),
                          google_pay_url) ||
          base::ContainsValue(spec_->url_payment_method_identifiers(),
                              android_pay_url),
      /*requested_method_other=*/non_google_it !=
          spec_->url_payment_method_identifiers().end());
}

void PaymentRequest::Show(bool is_user_gesture) {
  if (!IsInitialized()) {
    log_.Error("Attempted show without initialization");
    OnConnectionTerminated();
    return;
  }

  if (is_show_called_) {
    log_.Error("Attempted show twice");
    OnConnectionTerminated();
    return;
  }

  is_show_called_ = true;

  // A tab can display only one PaymentRequest UI at a time.
  display_handle_ = display_manager_->TryShow(delegate_.get());
  if (!display_handle_) {
    log_.Error("A PaymentRequest UI is already showing");
    journey_logger_.SetNotShown(
        JourneyLogger::NOT_SHOWN_REASON_CONCURRENT_REQUESTS);
    client_->OnError(mojom::PaymentErrorReason::ALREADY_SHOWING);
    OnConnectionTerminated();
    return;
  }

  if (!delegate_->IsBrowserWindowActive()) {
    log_.Error("Cannot show PaymentRequest UI in a background tab");
    journey_logger_.SetNotShown(JourneyLogger::NOT_SHOWN_REASON_OTHER);
    client_->OnError(mojom::PaymentErrorReason::USER_CANCEL);
    OnConnectionTerminated();
    return;
  }

  if (!state_) {
    // SSL is not valid. Reject show with NotSupportedError, disconnect the
    // mojo pipe, and destroy this object.
    AreRequestedMethodsSupportedCallback(false);
    return;
  }

  is_show_user_gesture_ = is_user_gesture;

  display_handle_->Show(this);

  state_->AreRequestedMethodsSupported(
      base::BindOnce(&PaymentRequest::AreRequestedMethodsSupportedCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PaymentRequest::Retry(mojom::PaymentValidationErrorsPtr errors) {
  if (!IsInitialized()) {
    log_.Error("Attempted retry without initialization");
    OnConnectionTerminated();
    return;
  }

  if (!IsThisPaymentRequestShowing()) {
    log_.Error("Attempted retry without show");
    OnConnectionTerminated();
    return;
  }

  std::string error;
  if (!PaymentsValidators::IsValidPaymentValidationErrorsFormat(errors,
                                                                &error)) {
    log_.Error(error);
    client_->OnError(mojom::PaymentErrorReason::USER_CANCEL);
    OnConnectionTerminated();
    return;
  }

  spec()->Retry(std::move(errors));
  display_handle_->Retry();
}

void PaymentRequest::UpdateWith(mojom::PaymentDetailsPtr details) {
  if (!IsInitialized()) {
    log_.Error("Attempted updateWith without initialization");
    OnConnectionTerminated();
    return;
  }

  if (!IsThisPaymentRequestShowing()) {
    log_.Error("Attempted updateWith without show");
    OnConnectionTerminated();
    return;
  }

  std::string error;
  if (!ValidatePaymentDetails(ConvertPaymentDetails(details), &error)) {
    log_.Error(error);
    OnConnectionTerminated();
    return;
  }

  if (details->shipping_address_errors &&
      !PaymentsValidators::IsValidAddressErrorsFormat(
          details->shipping_address_errors, &error)) {
    log_.Error(error);
    OnConnectionTerminated();
    return;
  }

  if (!details->total) {
    log_.Error("Missing total");
    OnConnectionTerminated();
    return;
  }

  spec_->UpdateWith(std::move(details));
}

void PaymentRequest::NoUpdatedPaymentDetails() {
  // This Mojo call is triggered by the user of the API doing nothing in
  // response to a shipping address update event, so the error messages cannot
  // be more verbose.
  if (!IsInitialized()) {
    log_.Error("Not initialized");
    OnConnectionTerminated();
    return;
  }

  if (!IsThisPaymentRequestShowing()) {
    log_.Error("Not shown");
    OnConnectionTerminated();
    return;
  }

  spec_->RecomputeSpecForDetails();
}

void PaymentRequest::Abort() {
  if (!IsInitialized()) {
    log_.Error("Attempted abort without initialization");
    OnConnectionTerminated();
    return;
  }

  if (!IsThisPaymentRequestShowing()) {
    log_.Error("Attempted abort without show");
    OnConnectionTerminated();
    return;
  }

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
  if (!IsInitialized()) {
    log_.Error("Attempted complete without initialization");
    OnConnectionTerminated();
    return;
  }

  if (!IsThisPaymentRequestShowing()) {
    log_.Error("Attempted complete without show");
    OnConnectionTerminated();
    return;
  }

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
  if (!IsInitialized()) {
    log_.Error("Attempted canMakePayment without initialization");
    OnConnectionTerminated();
    return;
  }

  // It's valid to call canMakePayment() without calling show() first.

  if (observer_for_testing_)
    observer_for_testing_->OnCanMakePaymentCalled();

  if (!delegate_->GetPrefService()->GetBoolean(kCanMakePaymentEnabled) ||
      !state_) {
    CanMakePaymentCallback(/*can_make_payment=*/false);
  } else {
    state_->CanMakePayment(
        base::BindOnce(&PaymentRequest::CanMakePaymentCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void PaymentRequest::AreRequestedMethodsSupportedCallback(
    bool methods_supported) {
  if (methods_supported) {
    if (SatisfiesSkipUIConstraints()) {
      skipped_payment_request_ui_ = true;
      Pay();
    }
  } else {
    journey_logger_.SetNotShown(
        JourneyLogger::NOT_SHOWN_REASON_NO_SUPPORTED_PAYMENT_METHOD);
    client_->OnError(mojom::PaymentErrorReason::NOT_SUPPORTED);
    if (observer_for_testing_)
      observer_for_testing_->OnNotSupportedError();
    OnConnectionTerminated();
  }
}

bool PaymentRequest::IsInitialized() const {
  return is_initialized_ && client_ && client_.is_bound() &&
         binding_.is_bound();
}

bool PaymentRequest::IsThisPaymentRequestShowing() const {
  return is_show_called_ && display_handle_ && spec_ && state_;
}

bool PaymentRequest::SatisfiesSkipUIConstraints() const {
  return base::FeatureList::IsEnabled(features::kWebPaymentsSingleAppUiSkip) &&
         base::FeatureList::IsEnabled(::features::kServiceWorkerPaymentApps) &&
         is_show_user_gesture_ && state()->is_get_all_instruments_finished() &&
         state()->available_instruments().size() == 1 &&
         spec()->stringified_method_data().size() == 1 &&
         !spec()->request_shipping() && !spec()->request_payer_name() &&
         !spec()->request_payer_phone() &&
         !spec()->request_payer_email()
         // Only allowing URL base payment apps to skip the payment sheet.
         && spec()->url_payment_method_identifiers().size() == 1;
}

void PaymentRequest::OnPaymentResponseAvailable(
    mojom::PaymentResponsePtr response) {
  journey_logger_.SetEventOccurred(
      JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);

  // Do not send invalid response to client.
  if (response->method_name.empty() || response->stringified_details.empty()) {
    RecordFirstAbortReason(
        JourneyLogger::ABORT_REASON_INSTRUMENT_DETAILS_ERROR);
    delegate_->ShowErrorMessage();
    return;
  }

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

void PaymentRequest::OnPayerInfoSelected(mojom::PayerDetailPtr payer_info) {
  client_->OnPayerDetailChange(std::move(payer_info));
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

void PaymentRequest::DidStartMainFrameNavigationToDifferentDocument(
    bool is_user_initiated) {
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
  journey_logger_.SetEventOccurred(JourneyLogger::EVENT_PAY_CLICKED);

  // Log the correct "selected instrument" metric according to type.
  DCHECK(state_->selected_instrument());
  JourneyLogger::Event selected_event =
      JourneyLogger::Event::EVENT_SELECTED_OTHER;
  switch (state_->selected_instrument()->type()) {
    case PaymentInstrument::Type::AUTOFILL:
      selected_event = JourneyLogger::Event::EVENT_SELECTED_CREDIT_CARD;
      break;
    case PaymentInstrument::Type::SERVICE_WORKER_APP:
      selected_event = JourneyLogger::Event::EVENT_SELECTED_OTHER;
      break;
    case PaymentInstrument::Type::NATIVE_MOBILE_APP:
      NOTREACHED();
      break;
  }
  journey_logger_.SetEventOccurred(selected_event);

  state_->GeneratePaymentResponse();
}

void PaymentRequest::HideIfNecessary() {
  display_handle_.reset();
}

void PaymentRequest::RecordDialogShownEventInJourneyLogger() {
  journey_logger_.SetEventOccurred(JourneyLogger::EVENT_SHOWN);
}

bool PaymentRequest::IsIncognito() const {
  return delegate_->IsIncognito();
}

void PaymentRequest::RecordFirstAbortReason(
    JourneyLogger::AbortReason abort_reason) {
  if (!has_recorded_completion_) {
    has_recorded_completion_ = true;
    journey_logger_.SetAborted(abort_reason);
  }
}

void PaymentRequest::CanMakePaymentCallback(bool can_make_payment) {
  if (!spec_ || CanMakePaymentQueryFactory::GetInstance()
                    ->GetForContext(web_contents_->GetBrowserContext())
                    ->CanQuery(top_level_origin_, frame_origin_,
                               spec_->stringified_method_data())) {
    RespondToCanMakePaymentQuery(can_make_payment, false);
  } else if (OriginSecurityChecker::IsOriginLocalhostOrFile(frame_origin_)) {
    RespondToCanMakePaymentQuery(can_make_payment, true);
  } else {
    client_->OnCanMakePayment(
        mojom::CanMakePaymentQueryResult::QUERY_QUOTA_EXCEEDED);
  }

  if (observer_for_testing_)
    observer_for_testing_->OnCanMakePaymentReturned();
}

void PaymentRequest::RespondToCanMakePaymentQuery(bool can_make_payment,
                                                  bool warn_localhost_or_file) {
  mojom::CanMakePaymentQueryResult positive =
      warn_localhost_or_file
          ? mojom::CanMakePaymentQueryResult::WARNING_CAN_MAKE_PAYMENT
          : mojom::CanMakePaymentQueryResult::CAN_MAKE_PAYMENT;
  mojom::CanMakePaymentQueryResult negative =
      warn_localhost_or_file
          ? mojom::CanMakePaymentQueryResult::WARNING_CANNOT_MAKE_PAYMENT
          : mojom::CanMakePaymentQueryResult::CANNOT_MAKE_PAYMENT;

  client_->OnCanMakePayment(can_make_payment ? positive : negative);
  journey_logger_.SetCanMakePaymentValue(can_make_payment);
}

}  // namespace payments
