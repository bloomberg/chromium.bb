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
  card_unmask_view_ = CardUnmaskPromptView::CreateAndShow(this);
}

void CardUnmaskPromptControllerImpl::OnVerificationResult(
    AutofillClient::GetRealPanResult result) {
  if (!card_unmask_view_)
    return;

  base::string16 error_message;
  bool allow_retry = true;
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
      allow_retry = false;
      break;
    }

    case AutofillClient::NETWORK_ERROR: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_NETWORK);
      allow_retry = false;
      break;
    }
  }

  card_unmask_view_->GotVerificationResult(error_message, allow_retry);
}

void CardUnmaskPromptControllerImpl::OnUnmaskDialogClosed() {
  card_unmask_view_ = nullptr;
  delegate_->OnUnmaskPromptClosed();
}

void CardUnmaskPromptControllerImpl::OnUnmaskResponse(
    const base::string16& cvc,
    const base::string16& exp_month,
    const base::string16& exp_year,
    bool should_store_pan) {
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
