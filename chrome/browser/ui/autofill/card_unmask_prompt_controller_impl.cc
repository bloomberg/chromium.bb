// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/card_unmask_prompt_controller_impl.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/risk_util.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

CardUnmaskPromptControllerImpl::CardUnmaskPromptControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      card_unmask_view_(nullptr),
      unmasking_result_(AutofillClient::TRY_AGAIN_FAILURE),
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

  pending_response_ = CardUnmaskDelegate::UnmaskResponse();
  LoadRiskFingerprint();
  card_ = card;
  delegate_ = delegate;
  card_unmask_view_ = CreateAndShowView();
  unmasking_result_ = AutofillClient::TRY_AGAIN_FAILURE;
  unmasking_number_of_attempts_ = 0;
  unmasking_initial_should_store_pan_ = GetStoreLocallyStartState();
  AutofillMetrics::LogUnmaskPromptEvent(AutofillMetrics::UNMASK_PROMPT_SHOWN);
}

bool CardUnmaskPromptControllerImpl::AllowsRetry(
    AutofillClient::GetRealPanResult result) {
  if (result == AutofillClient::NETWORK_ERROR
      || result == AutofillClient::PERMANENT_FAILURE) {
    return false;
  }
  return true;
}

void CardUnmaskPromptControllerImpl::OnVerificationResult(
    AutofillClient::GetRealPanResult result) {
  if (!card_unmask_view_)
    return;

  base::string16 error_message;
  unmasking_result_ = result;
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
  }

  AutofillMetrics::LogRealPanResult(result);
  unmasking_allow_retry_ = AllowsRetry(result);
  card_unmask_view_->GotVerificationResult(error_message,
                                           AllowsRetry(result));
}

void CardUnmaskPromptControllerImpl::OnUnmaskDialogClosed() {
  card_unmask_view_ = nullptr;
  LogOnCloseEvents();
  delegate_->OnUnmaskPromptClosed();
}

void CardUnmaskPromptControllerImpl::LogOnCloseEvents() {
  if (unmasking_number_of_attempts_ == 0) {
    AutofillMetrics::LogUnmaskPromptEvent(
        AutofillMetrics::UNMASK_PROMPT_CLOSED_NO_ATTEMPTS);
    return;
  }

  bool final_should_store_pan =
      user_prefs::UserPrefs::Get(web_contents_->GetBrowserContext())
          ->GetBoolean(prefs::kAutofillWalletImportStorageCheckboxState);

  if (unmasking_result_ == AutofillClient::SUCCESS) {
    AutofillMetrics::LogUnmaskPromptEvent(
        unmasking_number_of_attempts_ == 1
        ? AutofillMetrics::UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT
        : AutofillMetrics::UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS);
    if (final_should_store_pan) {
      AutofillMetrics::LogUnmaskPromptEvent(
          AutofillMetrics::UNMASK_PROMPT_SAVED_CARD_LOCALLY);
    }
  } else {
    AutofillMetrics::LogUnmaskPromptEvent(
        AllowsRetry(unmasking_result_)
        ? AutofillMetrics
              ::UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE
        : AutofillMetrics
              ::UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE);
  }

  // Tracking changes in local save preference.
  AutofillMetrics::UnmaskPromptEvent event;
  if (unmasking_initial_should_store_pan_ && final_should_store_pan) {
    event = AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_OUT;
  } else if (!unmasking_initial_should_store_pan_
             && !final_should_store_pan) {
    event = AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_IN;
  } else if (unmasking_initial_should_store_pan_
             && !final_should_store_pan) {
    event = AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_OUT;
  } else {
    event = AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_IN;
  }
  AutofillMetrics::LogUnmaskPromptEvent(event);
}

void CardUnmaskPromptControllerImpl::OnUnmaskResponse(
    const base::string16& cvc,
    const base::string16& exp_month,
    const base::string16& exp_year,
    bool should_store_pan) {
  unmasking_number_of_attempts_++;
  card_unmask_view_->DisableAndWaitForVerification();

  DCHECK(!cvc.empty());
  pending_response_.cvc = cvc;
  if (ShouldRequestExpirationDate()) {
    pending_response_.exp_month = exp_month;
    pending_response_.exp_year = exp_year;
  }
  pending_response_.should_store_pan = should_store_pan;
  // Remember the last choice the user made (on this device).
  user_prefs::UserPrefs::Get(web_contents_->GetBrowserContext())->SetBoolean(
      prefs::kAutofillWalletImportStorageCheckboxState, should_store_pan);

  if (!pending_response_.risk_data.empty())
    delegate_->OnUnmaskResponse(pending_response_);
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
  return card_.GetServerStatus() == CreditCard::EXPIRED;
}

bool CardUnmaskPromptControllerImpl::GetStoreLocallyStartState() const {
  // TODO(estade): Don't even offer to save on Linux? Offer to save but
  // default to false?
  PrefService* prefs =
      user_prefs::UserPrefs::Get(web_contents_->GetBrowserContext());
  return prefs->GetBoolean(prefs::kAutofillWalletImportStorageCheckboxState);
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
  LoadRiskData(
      0, web_contents_,
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
