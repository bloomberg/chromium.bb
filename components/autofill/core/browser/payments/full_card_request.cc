// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/full_card_request.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {
namespace payments {

FullCardRequest::FullCardRequest(AutofillClient* autofill_client,
                                 payments::PaymentsClient* payments_client,
                                 PersonalDataManager* personal_data_manager)
    : autofill_client_(autofill_client),
      payments_client_(payments_client),
      personal_data_manager_(personal_data_manager),
      delegate_(nullptr),
      weak_ptr_factory_(this) {
  DCHECK(autofill_client_);
  DCHECK(personal_data_manager_);
}

FullCardRequest::~FullCardRequest() {}

void FullCardRequest::GetFullCard(const CreditCard& card,
                                  AutofillClient::UnmaskCardReason reason,
                                  base::WeakPtr<Delegate> delegate) {
  DCHECK(delegate);

  // Only request can be active a time. If the member variable |delegate_| is
  // already set, then immediately reject the new request through the method
  // parameter |delegate|.
  if (delegate_) {
    delegate->OnFullCardError();
    return;
  }

  delegate_ = delegate;
  request_.reset(new payments::PaymentsClient::UnmaskRequestDetails);
  request_->card = card;
  bool is_masked = card.record_type() == CreditCard::MASKED_SERVER_CARD;
  if (is_masked)
    payments_client_->Prepare();

  autofill_client_->ShowUnmaskPrompt(request_->card, reason,
                                     weak_ptr_factory_.GetWeakPtr());

  if (is_masked) {
    autofill_client_->LoadRiskData(
        base::Bind(&FullCardRequest::OnDidGetUnmaskRiskData,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

bool FullCardRequest::IsGettingFullCard() const {
  return !!request_;
}

void FullCardRequest::OnUnmaskResponse(const UnmaskResponse& response) {
  // TODO(rouslan): Update the expiration date of the card on disk.
  // http://crbug.com/606008
  if (!response.exp_month.empty())
    request_->card.SetRawInfo(CREDIT_CARD_EXP_MONTH, response.exp_month);

  if (!response.exp_year.empty())
    request_->card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, response.exp_year);

  if (request_->card.record_type() != CreditCard::MASKED_SERVER_CARD) {
    if (delegate_)
      delegate_->OnFullCardDetails(request_->card, response.cvc);
    delegate_ = nullptr;
    autofill_client_->OnUnmaskVerificationResult(AutofillClient::SUCCESS);
    request_.reset();
    return;
  }

  request_->user_response = response;
  if (!request_->risk_data.empty()) {
    real_pan_request_timestamp_ = base::Time::Now();
    payments_client_->UnmaskCard(*request_);
  }
}

void FullCardRequest::OnUnmaskPromptClosed() {
  if (delegate_)
    delegate_->OnFullCardError();

  delegate_ = nullptr;
  payments_client_->CancelRequest();
  request_.reset();
}

void FullCardRequest::OnDidGetUnmaskRiskData(const std::string& risk_data) {
  request_->risk_data = risk_data;
  if (!request_->user_response.cvc.empty()) {
    real_pan_request_timestamp_ = base::Time::Now();
    payments_client_->UnmaskCard(*request_);
  }
}

void FullCardRequest::OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                                      const std::string& real_pan) {
  AutofillMetrics::LogRealPanDuration(
      base::Time::Now() - real_pan_request_timestamp_, result);

  if (!real_pan.empty()) {
    DCHECK_EQ(AutofillClient::SUCCESS, result);
    request_->card.set_record_type(CreditCard::FULL_SERVER_CARD);
    request_->card.SetNumber(base::UTF8ToUTF16(real_pan));

    if (request_->user_response.should_store_pan)
      personal_data_manager_->UpdateServerCreditCard(request_->card);

    if (delegate_)
      delegate_->OnFullCardDetails(request_->card, request_->user_response.cvc);
  } else {
    if (delegate_)
      delegate_->OnFullCardError();
  }

  delegate_ = nullptr;
  autofill_client_->OnUnmaskVerificationResult(result);
  request_.reset();
}

}  // namespace payments
}  // namespace autofill
