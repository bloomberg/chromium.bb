// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/card_unmask_prompt_controller_impl.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

CardUnmaskPromptControllerImpl::CardUnmaskPromptControllerImpl(
    content::WebContents* web_contents,
    const RiskDataCallback& risk_data_callback,
    PrefService* pref_service,
    bool is_off_the_record)
    : web_contents_(web_contents),
      risk_data_callback_(risk_data_callback),
      pref_service_(pref_service),
      new_card_link_clicked_(false),
      is_off_the_record_(is_off_the_record),
      card_unmask_view_(nullptr),
      unmasking_result_(AutofillClient::NONE),
      unmasking_initial_should_store_pan_(false),
      unmasking_number_of_attempts_(0),
      weak_pointer_factory_(this) {
}

CardUnmaskPromptControllerImpl::~CardUnmaskPromptControllerImpl() {
  if (card_unmask_view_)
    card_unmask_view_->ControllerGone();
}

void CardUnmaskPromptControllerImpl::ShowPrompt(
    const CreditCard& card,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  if (card_unmask_view_)
    card_unmask_view_->ControllerGone();

  new_card_link_clicked_ = false;
  shown_timestamp_ = base::Time::Now();
  pending_response_ = CardUnmaskDelegate::UnmaskResponse();
  LoadRiskFingerprint();
  card_ = card;
  delegate_ = delegate;
  card_unmask_view_ = CreateAndShowView();
  unmasking_result_ = AutofillClient::NONE;
  unmasking_number_of_attempts_ = 0;
  unmasking_initial_should_store_pan_ = GetStoreLocallyStartState();
  AutofillMetrics::LogUnmaskPromptEvent(AutofillMetrics::UNMASK_PROMPT_SHOWN);
}

bool CardUnmaskPromptControllerImpl::AllowsRetry(
    AutofillClient::GetRealPanResult result) {
  if (result == AutofillClient::NETWORK_ERROR ||
      result == AutofillClient::PERMANENT_FAILURE) {
    return false;
  }
  return true;
}

void CardUnmaskPromptControllerImpl::OnVerificationResult(
    AutofillClient::GetRealPanResult result) {
  if (!card_unmask_view_)
    return;

  base::string16 error_message;
  switch (result) {
    case AutofillClient::SUCCESS:
      break;

    case AutofillClient::TRY_AGAIN_FAILURE: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_TRY_AGAIN);
      break;
    }

    case AutofillClient::PERMANENT_FAILURE: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_PERMANENT);
      break;
    }

    case AutofillClient::NETWORK_ERROR: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_NETWORK);
      break;
    }

    case AutofillClient::NONE:
      NOTREACHED();
      return;
  }

  unmasking_result_ = result;
  AutofillMetrics::LogRealPanResult(result);
  AutofillMetrics::LogUnmaskingDuration(base::Time::Now() - verify_timestamp_,
                                        result);
  card_unmask_view_->GotVerificationResult(error_message,
                                           AllowsRetry(result));
}

void CardUnmaskPromptControllerImpl::OnUnmaskDialogClosed() {
  card_unmask_view_ = nullptr;
  LogOnCloseEvents();
  if (delegate_.get())
    delegate_->OnUnmaskPromptClosed();
}

void CardUnmaskPromptControllerImpl::LogOnCloseEvents() {
  AutofillMetrics::UnmaskPromptEvent close_reason_event = GetCloseReasonEvent();
  AutofillMetrics::LogUnmaskPromptEvent(close_reason_event);
  AutofillMetrics::LogUnmaskPromptEventDuration(
      base::Time::Now() - shown_timestamp_, close_reason_event);

  if (close_reason_event == AutofillMetrics::UNMASK_PROMPT_CLOSED_NO_ATTEMPTS)
    return;

  if (close_reason_event ==
      AutofillMetrics::UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING) {
    AutofillMetrics::LogTimeBeforeAbandonUnmasking(base::Time::Now() -
                                                   verify_timestamp_);
  }

  bool final_should_store_pan = pending_response_.should_store_pan;
  if (unmasking_result_ == AutofillClient::SUCCESS && final_should_store_pan) {
    AutofillMetrics::LogUnmaskPromptEvent(
        AutofillMetrics::UNMASK_PROMPT_SAVED_CARD_LOCALLY);
  }

  if (CanStoreLocally()) {
    // Tracking changes in local save preference.
    AutofillMetrics::UnmaskPromptEvent event;
    if (unmasking_initial_should_store_pan_ && final_should_store_pan) {
      event = AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_OUT;
    } else if (!unmasking_initial_should_store_pan_ &&
               !final_should_store_pan) {
      event = AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_IN;
    } else if (unmasking_initial_should_store_pan_ && !final_should_store_pan) {
      event = AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_OUT;
    } else {
      event = AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_IN;
    }
    AutofillMetrics::LogUnmaskPromptEvent(event);
  }
}

AutofillMetrics::UnmaskPromptEvent
CardUnmaskPromptControllerImpl::GetCloseReasonEvent() {
  if (unmasking_number_of_attempts_ == 0)
    return AutofillMetrics::UNMASK_PROMPT_CLOSED_NO_ATTEMPTS;

  // If NONE and we have a pending request, we have a pending GetRealPan
  // request.
  if (unmasking_result_ == AutofillClient::NONE)
    return AutofillMetrics::UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING;

  if (unmasking_result_ == AutofillClient::SUCCESS) {
    return unmasking_number_of_attempts_ == 1
               ? AutofillMetrics::UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT
               : AutofillMetrics::
                     UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS;
  } else {
    return AllowsRetry(unmasking_result_)
               ? AutofillMetrics::
                     UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE
               : AutofillMetrics::
                     UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE;
  }
}

void CardUnmaskPromptControllerImpl::OnUnmaskResponse(
    const base::string16& cvc,
    const base::string16& exp_month,
    const base::string16& exp_year,
    bool should_store_pan) {
  verify_timestamp_ = base::Time::Now();
  unmasking_number_of_attempts_++;
  unmasking_result_ = AutofillClient::NONE;
  card_unmask_view_->DisableAndWaitForVerification();

  DCHECK(InputCvcIsValid(cvc));
  base::TrimWhitespace(cvc, base::TRIM_ALL, &pending_response_.cvc);
  if (ShouldRequestExpirationDate()) {
    DCHECK(InputExpirationIsValid(exp_month, exp_year));
    pending_response_.exp_month = exp_month;
    pending_response_.exp_year = exp_year;
  }
  if (CanStoreLocally()) {
    pending_response_.should_store_pan = should_store_pan;
    // Remember the last choice the user made (on this device).
    pref_service_->SetBoolean(
        prefs::kAutofillWalletImportStorageCheckboxState, should_store_pan);
  } else {
    DCHECK(!should_store_pan);
    pending_response_.should_store_pan = false;
  }

  if (!pending_response_.risk_data.empty())
    delegate_->OnUnmaskResponse(pending_response_);
}

void CardUnmaskPromptControllerImpl::NewCardLinkClicked() {
  new_card_link_clicked_ = true;
}

content::WebContents* CardUnmaskPromptControllerImpl::GetWebContents() {
  return web_contents_;
}

base::string16 CardUnmaskPromptControllerImpl::GetWindowTitle() const {
  int ids = ShouldRequestExpirationDate()
      ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_UPDATE_TITLE
      : IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE;
  return l10n_util::GetStringFUTF16(ids, card_.TypeAndLastFourDigits());
}

base::string16 CardUnmaskPromptControllerImpl::GetInstructionsMessage() const {
  if (ShouldRequestExpirationDate()) {
    return l10n_util::GetStringUTF16(
        card_.type() == kAmericanExpressCard
            ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_EXPIRED_AMEX
            : IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_EXPIRED);
  }

  return l10n_util::GetStringUTF16(
      card_.type() == kAmericanExpressCard
          ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_AMEX
          : IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS);
}

int CardUnmaskPromptControllerImpl::GetCvcImageRid() const {
  return card_.type() == kAmericanExpressCard ? IDR_CREDIT_CARD_CVC_HINT_AMEX
                                              : IDR_CREDIT_CARD_CVC_HINT;
}

bool CardUnmaskPromptControllerImpl::ShouldRequestExpirationDate() const {
  return card_.GetServerStatus() == CreditCard::EXPIRED ||
         new_card_link_clicked_;
}

bool CardUnmaskPromptControllerImpl::CanStoreLocally() const {
  // Never offer to save for incognito.
  if (is_off_the_record_)
    return false;
  return autofill::OfferStoreUnmaskedCards();
}

bool CardUnmaskPromptControllerImpl::GetStoreLocallyStartState() const {
  return pref_service_->GetBoolean(
      prefs::kAutofillWalletImportStorageCheckboxState);
}

bool CardUnmaskPromptControllerImpl::InputCvcIsValid(
    const base::string16& input_text) const {
  base::string16 trimmed_text;
  base::TrimWhitespace(input_text, base::TRIM_ALL, &trimmed_text);
  size_t input_size = card_.type() == kAmericanExpressCard ? 4 : 3;
  return trimmed_text.size() == input_size &&
         base::ContainsOnlyChars(trimmed_text,
                                 base::ASCIIToUTF16("0123456789"));
}

bool CardUnmaskPromptControllerImpl::InputExpirationIsValid(
    const base::string16& month,
    const base::string16& year) const {
  if ((month.size() != 2U && month.size() != 1U) ||
      (year.size() != 4U && year.size() != 2U)) {
    return false;
  }

  int month_value = 0, year_value = 0;
  if (!base::StringToInt(month, &month_value) ||
      !base::StringToInt(year, &year_value)) {
    return false;
  }

  if (month_value < 1 || month_value > 12)
    return false;

  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);

  // Convert 2 digit year to 4 digit year.
  if (year_value < 100)
    year_value += (now.year / 100) * 100;

  if (year_value < now.year)
    return false;

  if (year_value > now.year)
    return true;

  return month_value >= now.month;
}

base::TimeDelta CardUnmaskPromptControllerImpl::GetSuccessMessageDuration()
    const {
  return base::TimeDelta::FromMilliseconds(500);
}

CardUnmaskPromptView* CardUnmaskPromptControllerImpl::CreateAndShowView() {
  return CardUnmaskPromptView::CreateAndShow(this);
}

void CardUnmaskPromptControllerImpl::LoadRiskFingerprint() {
  risk_data_callback_.Run(
      base::Bind(&CardUnmaskPromptControllerImpl::OnDidLoadRiskFingerprint,
                 weak_pointer_factory_.GetWeakPtr()));
}

void CardUnmaskPromptControllerImpl::OnDidLoadRiskFingerprint(
    const std::string& risk_data) {
  pending_response_.risk_data = risk_data;
  if (!pending_response_.cvc.empty())
    delegate_->OnUnmaskResponse(pending_response_);
}

}  // namespace autofill
