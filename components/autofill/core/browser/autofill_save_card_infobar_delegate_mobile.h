// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/legal_message_line.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class PrefService;

namespace base {
class DictionaryValue;
}

namespace autofill {

class CreditCard;

// An InfoBarDelegate that enables the user to allow or deny storing credit
// card information gathered from a form submission. Only used on mobile.
class AutofillSaveCardInfoBarDelegateMobile : public ConfirmInfoBarDelegate {
 public:
  AutofillSaveCardInfoBarDelegateMobile(
      bool upload,
      const CreditCard& card,
      std::unique_ptr<base::DictionaryValue> legal_message,
      const base::Closure& save_card_callback,
      PrefService* pref_service);

  ~AutofillSaveCardInfoBarDelegateMobile() override;

  int issuer_icon_id() const { return issuer_icon_id_; }
  const base::string16& card_label() const { return card_label_; }
  const base::string16& card_sub_label() const { return card_sub_label_; }
  const LegalMessageLines& legal_messages() const { return legal_messages_; }

  // Called when a link in the legal message text was clicked.
  void OnLegalMessageLinkClicked(GURL url);

  // Ensures the InfoBar is not shown if legal messages failed to parse.
  // Legal messages are only specified for the upload case, not for local save.
  bool LegalMessagesParsedSuccessfully();

  // Google Pay branding is enabled with a flag and only for cards upstreamed
  // to Google.
  bool IsGooglePayBrandingEnabled() const;

  // All following changes are with respect to Google Pay branding.
  base::string16 GetTitleText() const;
  base::string16 GetDescriptionText() const;

  // ConfirmInfoBarDelegate:
  int GetIconId() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetLinkText() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  GURL GetLinkURL() const override;

 private:
  void LogUserAction(AutofillMetrics::InfoBarMetric user_action);

  // Whether the action is an upload or a local save.
  bool upload_;

  // The callback to save credit card if the user accepts the infobar.
  base::Closure save_card_callback_;

  // Weak reference to read & write |kAutofillAcceptSaveCreditCardPromptState|,
  PrefService* pref_service_;

  // Did the user ever explicitly accept or dismiss this infobar?
  bool had_user_interaction_;

  // The resource ID for the icon that identifies the issuer of the card.
  int issuer_icon_id_;

  // The label for the card to show in the content of the infobar.
  base::string16 card_label_;

  // The sub-label for the card to show in the content of the infobar.
  base::string16 card_sub_label_;

  // The legal messages to show in the content of the infobar.
  LegalMessageLines legal_messages_;

  DISALLOW_COPY_AND_ASSIGN(AutofillSaveCardInfoBarDelegateMobile);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_
