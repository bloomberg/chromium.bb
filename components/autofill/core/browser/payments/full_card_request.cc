// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/full_card_request.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_tick_clock.h"

namespace autofill {
namespace payments {

FullCardRequest::FullCardRequest(RiskDataLoader* risk_data_loader,
                                 payments::PaymentsClient* payments_client,
                                 PersonalDataManager* personal_data_manager)
    : FullCardRequest(risk_data_loader,
                      payments_client,
                      personal_data_manager,
                      base::TimeTicks()) {}

FullCardRequest::FullCardRequest(RiskDataLoader* risk_data_loader,
                                 payments::PaymentsClient* payments_client,
                                 PersonalDataManager* personal_data_manager,
                                 base::TimeTicks form_parsed_timestamp)
    : risk_data_loader_(risk_data_loader),
      payments_client_(payments_client),
      personal_data_manager_(personal_data_manager),
      result_delegate_(nullptr),
      ui_delegate_(nullptr),
      should_unmask_card_(false),
      form_parsed_timestamp_(form_parsed_timestamp) {
  DCHECK(risk_data_loader_);
  DCHECK(payments_client_);
  DCHECK(personal_data_manager_);
}

FullCardRequest::~FullCardRequest() {}

void FullCardRequest::GetFullCard(const CreditCard& card,
                                  AutofillClient::UnmaskCardReason reason,
                                  base::WeakPtr<ResultDelegate> result_delegate,
                                  base::WeakPtr<UIDelegate> ui_delegate) {
  DCHECK(ui_delegate);
  GetFullCard(card, reason, result_delegate, ui_delegate,
              /*fido_assertion_info=*/base::nullopt);
}

void FullCardRequest::GetFullCardViaFIDO(
    const CreditCard& card,
    AutofillClient::UnmaskCardReason reason,
    base::WeakPtr<ResultDelegate> result_delegate,
    base::Value fido_assertion_info) {
  DCHECK(fido_assertion_info.is_dict());
  GetFullCard(card, reason, result_delegate, nullptr,
              std::move(fido_assertion_info));
}

void FullCardRequest::GetFullCard(
    const CreditCard& card,
    AutofillClient::UnmaskCardReason reason,
    base::WeakPtr<ResultDelegate> result_delegate,
    base::WeakPtr<UIDelegate> ui_delegate,
    base::Optional<base::Value> fido_assertion_info) {
  // Retrieval of card information should happen via CVC auth or FIDO, but not
  // both. Use |ui_delegate|'s existence as evidence of doing CVC auth and
  // |fido_assertion_info| as evidence of doing FIDO auth.
  DCHECK_NE(fido_assertion_info.has_value(), !!ui_delegate);
  DCHECK(result_delegate);

  // Only one request can be active at a time. If the member variable
  // |result_delegate_| is already set, then immediately reject the new request
  // through the method parameter |result_delegate_|.
  if (result_delegate_) {
    result_delegate_->OnFullCardRequestFailed();
    return;
  }

  result_delegate_ = result_delegate;
  request_.reset(new payments::PaymentsClient::UnmaskRequestDetails);
  request_->card = card;
  request_->reason = reason;
  should_unmask_card_ = card.record_type() == CreditCard::MASKED_SERVER_CARD ||
                        (card.record_type() == CreditCard::FULL_SERVER_CARD &&
                         card.ShouldUpdateExpiration(AutofillClock::Now()));
  if (should_unmask_card_) {
    payments_client_->Prepare();
    request_->billing_customer_number =
        GetBillingCustomerId(personal_data_manager_);
  }

  request_->fido_assertion_info = std::move(fido_assertion_info);
  ui_delegate_ = ui_delegate;

  // If there is a UI delegate, then perform a CVC check.
  // Otherwise, continue and use |fido_assertion_info| to unmask.
  if (ui_delegate_) {
    ui_delegate_->ShowUnmaskPrompt(request_->card, reason,
                                   weak_ptr_factory_.GetWeakPtr());
  }

  if (should_unmask_card_) {
    risk_data_loader_->LoadRiskData(
        base::BindOnce(&FullCardRequest::OnDidGetUnmaskRiskData,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void FullCardRequest::OnUnmaskPromptAccepted(
    const UserProvidedUnmaskDetails& user_response) {
  if (!user_response.exp_month.empty())
    request_->card.SetRawInfo(CREDIT_CARD_EXP_MONTH, user_response.exp_month);

  if (!user_response.exp_year.empty())
    request_->card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR,
                              user_response.exp_year);

  if (request_->card.record_type() == CreditCard::LOCAL_CARD &&
      !request_->card.guid().empty() &&
      (!user_response.exp_month.empty() || !user_response.exp_year.empty())) {
    personal_data_manager_->UpdateCreditCard(request_->card);
  }

  if (!should_unmask_card_) {
    if (result_delegate_)
      result_delegate_->OnFullCardRequestSucceeded(*this, request_->card,
                                                   user_response.cvc);
    if (ui_delegate_)
      ui_delegate_->OnUnmaskVerificationResult(AutofillClient::SUCCESS);
    Reset();

    return;
  }

  request_->user_response = user_response;
#if defined(OS_ANDROID)
  if (ui_delegate_) {
    // An opt-in request to Payments must be included either if the user chose
    // to opt-in through the CVC prompt or if the UI delegate indicates that the
    // user previously chose to opt-in through the settings page.
    request_->user_response.enable_fido_auth =
        user_response.enable_fido_auth ||
        ui_delegate_->UserOptedInToFidoFromSettingsPageOnMobile();
  }
#endif

  if (!request_->risk_data.empty())
    SendUnmaskCardRequest();
}

void FullCardRequest::OnUnmaskPromptClosed() {
  if (result_delegate_)
    result_delegate_->OnFullCardRequestFailed();

  Reset();
}

bool FullCardRequest::ShouldOfferFidoAuth() const {
  // FIDO opt-in is only handled from card unmask on mobile. Desktop platforms
  // provide a separate opt-in bubble.
#if defined(OS_ANDROID)
  return ui_delegate_ && ui_delegate_->ShouldOfferFidoAuth();
#else
  return false;
#endif
}

void FullCardRequest::OnDidGetUnmaskRiskData(const std::string& risk_data) {
  request_->risk_data = risk_data;
  if (!request_->user_response.cvc.empty() ||
      request_->fido_assertion_info.has_value()) {
    SendUnmaskCardRequest();
  }
}

void FullCardRequest::SendUnmaskCardRequest() {
  real_pan_request_timestamp_ = AutofillTickClock::NowTicks();
  payments_client_->UnmaskCard(*request_,
                               base::BindOnce(&FullCardRequest::OnDidGetRealPan,
                                              weak_ptr_factory_.GetWeakPtr()));
}

void FullCardRequest::OnDidGetRealPan(
    AutofillClient::PaymentsRpcResult result,
    payments::PaymentsClient::UnmaskResponseDetails& response_details) {
  // If the CVC field is populated, that means the user performed a CVC check.
  // If FIDO AssertionInfo is populated, then the user must have performed FIDO
  // authentication. Exactly one of these fields must be populated.
  DCHECK_NE(!request_->user_response.cvc.empty(),
            request_->fido_assertion_info.has_value());
  if (!request_->user_response.cvc.empty()) {
    AutofillMetrics::LogRealPanDuration(
        AutofillTickClock::NowTicks() - real_pan_request_timestamp_, result);
  } else if (request_->fido_assertion_info.has_value()) {
    AutofillMetrics::LogCardUnmaskDurationAfterWebauthn(
        AutofillTickClock::NowTicks() - real_pan_request_timestamp_, result);
  }

  if (ui_delegate_)
    ui_delegate_->OnUnmaskVerificationResult(result);

  switch (result) {
    // Wait for user retry.
    case AutofillClient::TRY_AGAIN_FAILURE:
      break;

    // Neither PERMANENT_FAILURE nor NETWORK_ERROR allow retry.
    case AutofillClient::PERMANENT_FAILURE:
    // Intentional fall through.
    case AutofillClient::NETWORK_ERROR: {
      if (result_delegate_)
        result_delegate_->OnFullCardRequestFailed();
      Reset();
      break;
    }

    case AutofillClient::SUCCESS: {
      DCHECK(!response_details.real_pan.empty());
      request_->card.set_record_type(CreditCard::FULL_SERVER_CARD);
      request_->card.SetNumber(base::UTF8ToUTF16(response_details.real_pan));
      request_->card.SetServerStatus(CreditCard::OK);

      if (request_->user_response.should_store_pan)
        personal_data_manager_->UpdateServerCreditCard(request_->card);

      // TODO(crbug/949269): Once |fido_opt_in| is added to
      // UserProvidedUnmaskDetails, clear out |creation_options| from
      // |response_details_| if |user_response.fido_opt_in| was not set to true
      // to avoid an unwanted registration prompt.
      unmask_response_details_ = response_details;

      const base::string16 cvc =
          (base::FeatureList::IsEnabled(
               features::kAutofillEnableGoogleIssuedCard) ||
           base::FeatureList::IsEnabled(
               features::kAutofillAlwaysReturnCloudTokenizedCard)) &&
                  !response_details.dcvv.empty()
              ? base::UTF8ToUTF16(response_details.dcvv)
              : request_->user_response.cvc;
      if (result_delegate_)
        result_delegate_->OnFullCardRequestSucceeded(*this, request_->card,
                                                     cvc);
      Reset();
      break;
    }

    case AutofillClient::NONE:
      NOTREACHED();
      break;
  }
}

void FullCardRequest::OnFIDOVerificationCancelled() {
  Reset();
}

void FullCardRequest::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  payments_client_->CancelRequest();
  result_delegate_ = nullptr;
  ui_delegate_ = nullptr;
  request_.reset();
  should_unmask_card_ = false;
  unmask_response_details_ = payments::PaymentsClient::UnmaskResponseDetails();
}

}  // namespace payments
}  // namespace autofill
