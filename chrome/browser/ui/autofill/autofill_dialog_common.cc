// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_common.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"

namespace autofill {
namespace common {

bool ServerTypeEncompassesFieldType(ServerFieldType type,
                                    const AutofillType& field_type) {
  // If any credit card expiration info is asked for, show both month and year
  // inputs.
  ServerFieldType server_type = field_type.GetStorableType();
  if (server_type == CREDIT_CARD_EXP_4_DIGIT_YEAR ||
      server_type == CREDIT_CARD_EXP_2_DIGIT_YEAR ||
      server_type == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR ||
      server_type == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
      server_type == CREDIT_CARD_EXP_MONTH) {
    return type == CREDIT_CARD_EXP_4_DIGIT_YEAR ||
           type == CREDIT_CARD_EXP_MONTH;
  }

  if (server_type == CREDIT_CARD_TYPE)
    return type == CREDIT_CARD_NUMBER;

  // Check the groups to distinguish billing types from shipping ones.
  AutofillType autofill_type = AutofillType(type);
  if (autofill_type.group() != field_type.group())
    return false;

  // The page may ask for individual address lines; this roughly matches the
  // street address blob.
  if (server_type == ADDRESS_HOME_LINE1 ||
          server_type == ADDRESS_HOME_LINE2 ||
          server_type == ADDRESS_HOME_LINE3) {
    return autofill_type.GetStorableType() == ADDRESS_HOME_STREET_ADDRESS;
  }

  // First, middle and last name are parsed from full name.
  if (field_type.group() == NAME || field_type.group() == NAME_BILLING)
    return autofill_type.GetStorableType() == NAME_FULL;

  return autofill_type.GetStorableType() == server_type;
}

bool ServerTypeMatchesField(DialogSection section,
                            ServerFieldType type,
                            const AutofillField& field) {
  AutofillType field_type = field.Type();

  // The credit card name is filled from the billing section's data.
  if (field_type.GetStorableType() == CREDIT_CARD_NAME &&
      (section == SECTION_BILLING || section == SECTION_CC_BILLING)) {
    return type == NAME_BILLING_FULL;
  }

  return ServerTypeEncompassesFieldType(type, field_type);
}

bool IsCreditCardType(ServerFieldType type) {
  return AutofillType(type).group() == CREDIT_CARD;
}

void BuildInputs(const DetailInput* input_template,
                 size_t template_size,
                 DetailInputs* inputs) {
  for (size_t i = 0; i < template_size; ++i) {
    const DetailInput* input = &input_template[i];
    inputs->push_back(*input);
  }
}

AutofillMetrics::DialogUiEvent DialogSectionToUiItemAddedEvent(
    DialogSection section) {
  switch (section) {
    case SECTION_BILLING:
      return AutofillMetrics::DIALOG_UI_BILLING_ITEM_ADDED;

    case SECTION_CC_BILLING:
      return AutofillMetrics::DIALOG_UI_CC_BILLING_ITEM_ADDED;

    case SECTION_SHIPPING:
      return AutofillMetrics::DIALOG_UI_SHIPPING_ITEM_ADDED;

    case SECTION_CC:
      return AutofillMetrics::DIALOG_UI_CC_ITEM_ADDED;
  }

  NOTREACHED();
  return AutofillMetrics::NUM_DIALOG_UI_EVENTS;
}

AutofillMetrics::DialogUiEvent DialogSectionToUiSelectionChangedEvent(
    DialogSection section) {
  switch (section) {
    case SECTION_BILLING:
      return AutofillMetrics::DIALOG_UI_BILLING_SELECTED_SUGGESTION_CHANGED;

    case SECTION_CC_BILLING:
      return AutofillMetrics::DIALOG_UI_CC_BILLING_SELECTED_SUGGESTION_CHANGED;

    case SECTION_SHIPPING:
      return AutofillMetrics::DIALOG_UI_SHIPPING_SELECTED_SUGGESTION_CHANGED;

    case SECTION_CC:
      return AutofillMetrics::DIALOG_UI_CC_SELECTED_SUGGESTION_CHANGED;
  }

  NOTREACHED();
  return AutofillMetrics::NUM_DIALOG_UI_EVENTS;
}

std::vector<ServerFieldType> TypesFromInputs(const DetailInputs& inputs) {
  std::vector<ServerFieldType> types;
  for (size_t i = 0; i < inputs.size(); ++i) {
    types.push_back(inputs[i].type);
  }
  return types;
}

}  // namespace common
}  // namespace autofill
