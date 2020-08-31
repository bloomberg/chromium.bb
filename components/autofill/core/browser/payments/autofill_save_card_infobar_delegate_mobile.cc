// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/grit/components_scaled_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace autofill {

AutofillSaveCardInfoBarDelegateMobile::AutofillSaveCardInfoBarDelegateMobile(
    bool upload,
    AutofillClient::SaveCreditCardOptions options,
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    AutofillClient::UploadSaveCardPromptCallback
        upload_save_card_prompt_callback,
    AutofillClient::LocalSaveCardPromptCallback local_save_card_prompt_callback,
    PrefService* pref_service)
    : ConfirmInfoBarDelegate(),
      upload_(upload),
      options_(options),
      upload_save_card_prompt_callback_(
          std::move(upload_save_card_prompt_callback)),
      local_save_card_prompt_callback_(
          std::move(local_save_card_prompt_callback)),
      pref_service_(pref_service),
      had_user_interaction_(false),
      issuer_icon_id_(CreditCard::IconResourceId(card.network())),
      card_label_(card.NetworkAndLastFourDigits()),
      card_sub_label_(card.AbbreviatedExpirationDateForDisplay(false)),
      card_last_four_digits_(card.LastFourDigits()),
      cardholder_name_(card.GetRawInfo(CREDIT_CARD_NAME_FULL)),
      expiration_date_month_(card.Expiration2DigitMonthAsString()),
      expiration_date_year_(card.Expiration4DigitYearAsString()),
      legal_message_lines_(legal_message_lines) {
  DCHECK_EQ(upload, !upload_save_card_prompt_callback_.is_null());
  DCHECK_EQ(upload, local_save_card_prompt_callback_.is_null());

  AutofillMetrics::LogCreditCardInfoBarMetric(
      AutofillMetrics::INFOBAR_SHOWN, upload_, options_,
      pref_service_->GetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState));
}

AutofillSaveCardInfoBarDelegateMobile::
    ~AutofillSaveCardInfoBarDelegateMobile() {
  if (!had_user_interaction_) {
    RunSaveCardPromptCallback(AutofillClient::IGNORED,
                              /*user_provided_details=*/{});
    LogUserAction(AutofillMetrics::INFOBAR_IGNORED);
  }
}

void AutofillSaveCardInfoBarDelegateMobile::OnLegalMessageLinkClicked(
    GURL url) {
  infobar()->owner()->OpenURL(url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

bool AutofillSaveCardInfoBarDelegateMobile::IsGooglePayBrandingEnabled() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return upload_;
#else
  return false;
#endif
}

base::string16 AutofillSaveCardInfoBarDelegateMobile::GetDescriptionText()
    const {
  // Without Google Pay branding, the title acts as the description (see
  // |GetMessageText|).
  if (!IsGooglePayBrandingEnabled())
    return base::string16();

  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3);
}

int AutofillSaveCardInfoBarDelegateMobile::GetIconId() const {
  return IsGooglePayBrandingEnabled() ? IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER
                                      : IDR_INFOBAR_AUTOFILL_CC;
}

base::string16 AutofillSaveCardInfoBarDelegateMobile::GetMessageText() const {
  return l10n_util::GetStringUTF16(
      IsGooglePayBrandingEnabled()
          ? IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3
          : upload_ ? IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD
                    : IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL);
}

infobars::InfoBarDelegate::InfoBarIdentifier
AutofillSaveCardInfoBarDelegateMobile::GetIdentifier() const {
  return AUTOFILL_CC_INFOBAR_DELEGATE_MOBILE;
}

bool AutofillSaveCardInfoBarDelegateMobile::ShouldExpire(
    const NavigationDetails& details) const {
#if defined(OS_IOS)
  if (base::FeatureList::IsEnabled(
          features::kAutofillSaveCardDismissOnNavigation)) {
    // Expire the Infobar unless the navigation was triggered by the form that
    // presented the Infobar, or the navigation is a redirect.
    return !details.is_form_submission && !details.is_redirect;
  } else {
    // Use the default behavior used by Android.
    return false;
  }
#else   // defined(OS_IOS)
  // The user has submitted a form, causing the page to navigate elsewhere. We
  // don't want the infobar to be expired at this point, because the user won't
  // get a chance to answer the question.
  return false;
#endif  // defined(OS_IOS)
}

void AutofillSaveCardInfoBarDelegateMobile::InfoBarDismissed() {
  RunSaveCardPromptCallback(AutofillClient::DECLINED,
                            /*user_provided_details=*/{});
  LogUserAction(AutofillMetrics::INFOBAR_DENIED);
}

bool AutofillSaveCardInfoBarDelegateMobile::Cancel() {
  RunSaveCardPromptCallback(AutofillClient::DECLINED,
                            /*user_provided_details=*/{});
  LogUserAction(AutofillMetrics::INFOBAR_DENIED);
  return true;
}

int AutofillSaveCardInfoBarDelegateMobile::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

base::string16 AutofillSaveCardInfoBarDelegateMobile::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    // Requesting name or expiration date from the user makes the save prompt a
    // 2-step fix flow.
    bool prompt_continue = options_.should_request_name_from_user ||
                           options_.should_request_expiration_date_from_user;
    return l10n_util::GetStringUTF16(
        prompt_continue ? IDS_AUTOFILL_SAVE_CARD_PROMPT_CONTINUE
                        : IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT);
  }

  if (button == BUTTON_CANCEL) {
    return l10n_util::GetStringUTF16(
        upload_ ? IDS_AUTOFILL_NO_THANKS_MOBILE_UPLOAD_SAVE
                : IDS_AUTOFILL_NO_THANKS_MOBILE_LOCAL_SAVE);
  }

  NOTREACHED() << "Unsupported button label requested.";
  return base::string16();
}

bool AutofillSaveCardInfoBarDelegateMobile::Accept() {
  RunSaveCardPromptCallback(AutofillClient::ACCEPTED,
                            /*user_provided_details=*/{});
  LogUserAction(AutofillMetrics::INFOBAR_ACCEPTED);
  return true;
}

#if defined(OS_IOS)
bool AutofillSaveCardInfoBarDelegateMobile::UpdateAndAccept(
    base::string16 cardholder_name,
    base::string16 expiration_date_month,
    base::string16 expiration_date_year) {
  AutofillClient::UserProvidedCardDetails user_provided_details;
  user_provided_details.cardholder_name = cardholder_name;
  user_provided_details.expiration_date_month = expiration_date_month;
  user_provided_details.expiration_date_year = expiration_date_year;
  RunSaveCardPromptCallback(AutofillClient::ACCEPTED, user_provided_details);
  LogUserAction(AutofillMetrics::INFOBAR_ACCEPTED);
  return true;
}
#endif  // defined(OS_IOS)

void AutofillSaveCardInfoBarDelegateMobile::RunSaveCardPromptCallback(
    AutofillClient::SaveCardOfferUserDecision user_decision,
    AutofillClient::UserProvidedCardDetails user_provided_details) {
  if (upload_) {
    std::move(upload_save_card_prompt_callback_)
        .Run(user_decision, user_provided_details);
  } else {
    std::move(local_save_card_prompt_callback_).Run(user_decision);
  }
}

void AutofillSaveCardInfoBarDelegateMobile::LogUserAction(
    AutofillMetrics::InfoBarMetric user_action) {
  DCHECK(!had_user_interaction_);

  AutofillMetrics::LogCreditCardInfoBarMetric(
      user_action, upload_, options_,
      pref_service_->GetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState));
  pref_service_->SetInteger(
      prefs::kAutofillAcceptSaveCreditCardPromptState,
      user_action == AutofillMetrics::INFOBAR_ACCEPTED
          ? prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_ACCEPTED
          : prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_DENIED);
  had_user_interaction_ = true;
}

}  // namespace autofill
