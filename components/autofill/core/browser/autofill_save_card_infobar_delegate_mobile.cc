// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_save_card_infobar_delegate_mobile.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/legal_message_line.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace autofill {

AutofillSaveCardInfoBarDelegateMobile::AutofillSaveCardInfoBarDelegateMobile(
    bool upload,
    const CreditCard& card,
    scoped_ptr<base::DictionaryValue> legal_message,
    const base::Closure& save_card_callback)
    : AutofillCCInfoBarDelegate(upload, save_card_callback),
#if defined(OS_IOS)
      // TODO(jdonnelly): Use credit card issuer images on iOS.
      // http://crbug.com/535784
      issuer_icon_id_(kNoIconID),
#else
      issuer_icon_id_(CreditCard::IconResourceId(
          CreditCard::GetCreditCardType(card.GetRawInfo(CREDIT_CARD_NUMBER)))),
#endif
      card_label_(base::string16(kMidlineEllipsis) + card.LastFourDigits()),
      card_sub_label_(card.AbbreviatedExpirationDateForDisplay()) {
  if (legal_message)
    LegalMessageLine::Parse(*legal_message, &legal_messages_);
}

AutofillSaveCardInfoBarDelegateMobile::
    ~AutofillSaveCardInfoBarDelegateMobile() {}

void AutofillSaveCardInfoBarDelegateMobile::OnLegalMessageLinkClicked(
    GURL url) {
  infobar()->owner()->OpenURL(url, NEW_FOREGROUND_TAB);
}

}  // namespace autofill
