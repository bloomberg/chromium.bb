// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_CARD_NAME_FIX_FLOW_VIEW_DELEGATE_MOBILE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_CARD_NAME_FIX_FLOW_VIEW_DELEGATE_MOBILE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/legal_message_line.h"

namespace base {
class DictionaryValue;
}

namespace autofill {

// Enables the user to accept or deny cardholder name fix flow prompt.
// Only used on mobile.
class CardNameFixFlowViewDelegateMobile {
 public:
  CardNameFixFlowViewDelegateMobile(
      const base::string16& inferred_cardholder_name,
      std::unique_ptr<base::DictionaryValue> legal_message,
      base::OnceCallback<void(const base::string16&)>
          upload_save_card_callback);

  ~CardNameFixFlowViewDelegateMobile();

  const LegalMessageLines& GetLegalMessageLines() const {
    return legal_messages_;
  }

  int GetIconId() const;
  base::string16 GetTitleText() const;
  base::string16 GetInferredCardHolderName() const;
  base::string16 GetSaveButtonLabel() const;
  void Accept(const base::string16& name);

 private:
  // Inferred cardholder name from Gaia account.
  base::string16 inferred_cardholder_name_;

  // The callback to save the credit card to Google Payments once user accepts
  // fix flow.
  base::OnceCallback<void(const base::string16&)> upload_save_card_callback_;

  // The legal messages to show in the fix flow.
  LegalMessageLines legal_messages_;

  DISALLOW_COPY_AND_ASSIGN(CardNameFixFlowViewDelegateMobile);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_CARD_NAME_FIX_FLOW_VIEW_DELEGATE_MOBILE_H_
