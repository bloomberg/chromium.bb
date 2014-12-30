// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/card_unmask_prompt_controller_impl.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/card_unmask_delegate.h"
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

  card_ = card;
  delegate_ = delegate;
  card_unmask_view_ = CardUnmaskPromptView::CreateAndShow(this);
}

void CardUnmaskPromptControllerImpl::OnVerificationResult(bool success) {
  if (card_unmask_view_)
    card_unmask_view_->GotVerificationResult(success);
}

void CardUnmaskPromptControllerImpl::OnUnmaskDialogClosed() {
  card_unmask_view_ = nullptr;
}

void CardUnmaskPromptControllerImpl::OnUnmaskResponse(
    const base::string16& cvc) {
  card_unmask_view_->DisableAndWaitForVerification();
  delegate_->OnUnmaskResponse(cvc);
}

content::WebContents* CardUnmaskPromptControllerImpl::GetWebContents() {
  return web_contents_;
}

base::string16 CardUnmaskPromptControllerImpl::GetWindowTitle() const {
  return base::ASCIIToUTF16("Unlocking ") + card_.TypeAndLastFourDigits();
}

base::string16 CardUnmaskPromptControllerImpl::GetInstructionsMessage() const {
  return l10n_util::GetStringUTF16(
      card_.type() == kAmericanExpressCard
          ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_AMEX
          : IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS);
}

int CardUnmaskPromptControllerImpl::GetCvcImageRid() const {
  return card_.type() == kAmericanExpressCard ? IDR_CREDIT_CARD_CVC_HINT_AMEX
                                              : IDR_CREDIT_CARD_CVC_HINT;
}

bool CardUnmaskPromptControllerImpl::InputTextIsValid(
    const base::string16& input_text) const {
  base::string16 trimmed_text;
  base::TrimWhitespace(input_text, base::TRIM_ALL, &trimmed_text);
  size_t input_size = card_.type() == kAmericanExpressCard ? 4 : 3;
  return trimmed_text.size() == input_size &&
         base::ContainsOnlyChars(trimmed_text,
                                 base::ASCIIToUTF16("0123456789"));
}

}  // namespace autofill
