// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/credit_card_editor_view_controller.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/preselected_combobox_model.h"
#include "chrome/browser/ui/views/payments/validating_combobox.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/payments/payment_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/textfield/textfield.h"

namespace payments {

namespace {

// Number of years (including the current one) to be shown in the expiration
// year dropdown.
const int kNumberOfExpirationYears = 10;

// Returns the items that are in the expiration month dropdown. Will return the
// months in order starting at "01" until "12". Uses a clock so that the
// |default_index| is set to the current month.
std::vector<base::string16> GetExpirationMonthItems(int* default_index) {
  std::vector<base::string16> months;
  months.reserve(12);
  for (int i = 1; i <= 12; i++)
    months.push_back(base::UTF8ToUTF16(base::StringPrintf("%02d", i)));

  base::Time::Exploded now_exploded;
  autofill::AutofillClock::Now().LocalExplode(&now_exploded);
  *default_index = now_exploded.month - 1;

  return months;
}

// Returns the items that are in the expiration year dropdown, with the first
// year being the current year.
std::vector<base::string16> GetExpirationYearItems() {
  std::vector<base::string16> years;
  years.reserve(kNumberOfExpirationYears);

  base::Time::Exploded now_exploded;
  autofill::AutofillClock::Now().LocalExplode(&now_exploded);
  for (int i = 0; i < kNumberOfExpirationYears; i++) {
    years.push_back(base::UTF8ToUTF16(std::to_string(now_exploded.year + i)));
  }
  return years;
}

}  // namespace

CreditCardEditorViewController::CreditCardEditorViewController(
    PaymentRequest* request,
    PaymentRequestDialogView* dialog)
    : EditorViewController(request, dialog) {}

CreditCardEditorViewController::~CreditCardEditorViewController() {}

std::vector<EditorField> CreditCardEditorViewController::GetFieldDefinitions() {
  return std::vector<EditorField>{
      {autofill::CREDIT_CARD_NAME_FULL,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_NAME_ON_CARD),
       EditorField::LengthHint::HINT_LONG, /* required= */ true},
      {autofill::CREDIT_CARD_NUMBER,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_CREDIT_CARD_NUMBER),
       EditorField::LengthHint::HINT_LONG, /* required= */ true},
      {autofill::CREDIT_CARD_EXP_MONTH,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_EXPIRATION_MONTH),
       EditorField::LengthHint::HINT_SHORT, /* required= */ true,
       EditorField::ControlType::COMBOBOX},
      {autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_EXPIRATION_YEAR),
       EditorField::LengthHint::HINT_SHORT, /* required= */ true,
       EditorField::ControlType::COMBOBOX}};
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
  for (const auto& field : comboboxes()) {
    // ValidatingCombobox* is the key, EditorField is the value.
    DCHECK_EQ(autofill::CREDIT_CARD,
              autofill::AutofillType(field.second.type).group());
    ValidatingCombobox* combobox = field.first;
    if (combobox->invalid())
      return false;

    credit_card.SetRawInfo(field.second.type,
                           combobox->GetTextForRow(combobox->selected_index()));
  }

  // TODO(mathp): Display global error message.
  if (!credit_card.IsValid())
    return false;

  // Add the card (will not add a duplicate).
  request()->personal_data_manager()->AddCreditCard(credit_card);

  return true;
}

std::unique_ptr<ValidationDelegate>
CreditCardEditorViewController::CreateValidationDelegate(
    const EditorField& field) {
  return base::MakeUnique<
      CreditCardEditorViewController::CreditCardValidationDelegate>(field);
}

std::unique_ptr<ui::ComboboxModel>
CreditCardEditorViewController::GetComboboxModelForType(
    const autofill::ServerFieldType& type) {
  switch (type) {
    case autofill::CREDIT_CARD_EXP_MONTH: {
      int default_index = 0;
      std::vector<base::string16> months =
          GetExpirationMonthItems(&default_index);
      return std::unique_ptr<ui::ComboboxModel>(
          new PreselectedComboboxModel(months, default_index));
    }
    case autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return std::unique_ptr<ui::ComboboxModel>(
          new ui::SimpleComboboxModel(GetExpirationYearItems()));
    default:
      NOTREACHED();
      break;
  }
  return std::unique_ptr<ui::ComboboxModel>();
}

CreditCardEditorViewController::CreditCardValidationDelegate::
    CreditCardValidationDelegate(const EditorField& field)
    : field_(field) {}
CreditCardEditorViewController::CreditCardValidationDelegate::
    ~CreditCardValidationDelegate() {}

bool CreditCardEditorViewController::CreditCardValidationDelegate::
    ValidateTextfield(views::Textfield* textfield) {
  return ValidateValue(textfield->text());
}

bool CreditCardEditorViewController::CreditCardValidationDelegate::
    ValidateCombobox(views::Combobox* combobox) {
  return ValidateValue(combobox->GetTextForRow(combobox->selected_index()));
}

bool CreditCardEditorViewController::CreditCardValidationDelegate::
    ValidateValue(const base::string16& value) {
  if (!value.empty()) {
    base::string16 error_message;
    // TODO(mathp): Display |error_message| around |textfield|.
    return autofill::IsValidForType(value, field_.type, &error_message);
  }

  // TODO(mathp): Display "required" error if applicable.
  return !field_.required;
}

}  // namespace payments
