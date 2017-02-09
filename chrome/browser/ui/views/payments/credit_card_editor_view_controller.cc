// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/credit_card_editor_view_controller.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/payments/payment_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/textfield/textfield.h"

namespace payments {

CreditCardEditorViewController::CreditCardEditorViewController(
    PaymentRequest* request,
    PaymentRequestDialogView* dialog)
    : EditorViewController(request, dialog) {}

CreditCardEditorViewController::~CreditCardEditorViewController() {}

std::vector<EditorField> CreditCardEditorViewController::GetFieldDefinitions() {
  return std::vector<EditorField>{
      {autofill::CREDIT_CARD_NAME_FULL,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_NAME_ON_CARD),
       EditorField::LengthHint::HINT_LONG},
      {autofill::CREDIT_CARD_NUMBER,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_CREDIT_CARD_NUMBER),
       EditorField::LengthHint::HINT_LONG},
      {autofill::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_EXPIRATION_DATE),
       EditorField::LengthHint::HINT_SHORT}};
}

bool CreditCardEditorViewController::ValidateModelAndSave() {
  autofill::CreditCard credit_card;
  credit_card.set_origin(autofill::kSettingsOrigin);
  for (const auto& field : text_fields()) {
    // ValidatingTextfield* is the key, EditorField is the value.
    DCHECK_EQ(autofill::CREDIT_CARD,
              autofill::AutofillType(field.second.type).group());
    if (field.first->invalid())
      return false;

    credit_card.SetRawInfo(field.second.type, field.first->text());
  }

  // TODO(mathp): Display global error message.
  if (!credit_card.IsValid())
    return false;

  // Add the card (will not add a duplicate).
  request()->personal_data_manager()->AddCreditCard(credit_card);

  return true;
}

std::unique_ptr<ValidatingTextfield::Delegate>
CreditCardEditorViewController::CreateValidationDelegate(
    const EditorField& field) {
  return base::MakeUnique<CreditCardEditorViewController::ValidationDelegate>(
      field);
}

CreditCardEditorViewController::ValidationDelegate::ValidationDelegate(
    const EditorField& field)
    : field_(field) {}
CreditCardEditorViewController::ValidationDelegate::~ValidationDelegate() {}

bool CreditCardEditorViewController::ValidationDelegate::ValidateTextfield(
    views::Textfield* textfield) {
  base::string16 error_message;
  // TODO(mathp): Display |error_message| around |textfield|.
  return autofill::IsValidForType(textfield->text(), field_.type,
                                  &error_message);
}

}  // namespace payments
