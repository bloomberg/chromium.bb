// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/credit_card_editor_view_controller.h"

#include <utility>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/payments/payment_request.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

CreditCardEditorViewController::CreditCardEditorViewController(
    PaymentRequest* request,
    PaymentRequestDialogView* dialog)
    : EditorViewController(request, dialog) {}

CreditCardEditorViewController::~CreditCardEditorViewController() {}

std::vector<EditorField> CreditCardEditorViewController::GetFieldDefinitions() {
  return std::vector<EditorField>{
      {autofill::CREDIT_CARD_NUMBER,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_CREDIT_CARD_NUMBER),
       EditorField::LengthHint::HINT_LONG},
      {autofill::CREDIT_CARD_NAME_FULL,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_NAME_ON_CARD),
       EditorField::LengthHint::HINT_LONG},
      {autofill::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_EXPIRATION_DATE),
       EditorField::LengthHint::HINT_SHORT}};
}

bool CreditCardEditorViewController::ValidateModelAndSave() {
  // TODO(mathp): Actual validation and saving the model on disk.
  return true;
}

}  // namespace payments
