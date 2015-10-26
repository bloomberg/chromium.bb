// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_agent_utils.h"

#include "base/bind.h"
#include "base/macros.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/detail_input.h"
#include "components/autofill/core/browser/dialog_section.h"
#include "components/autofill/core/browser/server_field_types_util.h"
#include "grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ui/base/l10n/l10n_util.h"

// TODO (sgrant): Switch to componentized version of this code when
// http://crbug/328070 is fixed.
// This code was largely copied from autofill_dialog_controller_android.cc

namespace {

// Returns true if |input_type| in |section| is needed for |form_structure|.
bool IsSectionInputUsedInFormStructure(
    autofill::DialogSection section,
    autofill::ServerFieldType input_type,
    const autofill::FormStructure& form_structure) {
  autofill::DetailInput input;
  input.length = autofill::DetailInput::SHORT;
  input.type = input_type;
  input.placeholder_text = base::string16();
  input.expand_weight = 0;

  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    const autofill::AutofillField* field = form_structure.field(i);
    if (field && autofill::ServerTypeMatchesField(section, input.type, *field))
      return true;
  }
  return false;
}

}  // namespace

namespace autofill_agent_util {

// Determines if the |structure| has any fields that are of type
// autofill::CREDIT_CARD and thus asking for credit card info.
bool RequestingCreditCardInfo(const autofill::FormStructure* structure) {
  DCHECK(structure);

  size_t field_count = structure->field_count();
  for (size_t i = 0; i < field_count; ++i) {
    autofill::AutofillType type(structure->field(i)->Type().GetStorableType());
    if (type.group() == autofill::CREDIT_CARD)
      return true;
  }

  return false;
}

// Returns true if one of the nodes in |structure| request information related
// to a billing address.
bool RequestFullBillingAddress(autofill::FormStructure* structure) {
  const autofill::ServerFieldType fieldsToCheckFor[] = {
      autofill::ADDRESS_BILLING_LINE1,
      autofill::ADDRESS_BILLING_LINE2,
      autofill::ADDRESS_BILLING_CITY,
      autofill::ADDRESS_BILLING_STATE,
      autofill::PHONE_BILLING_WHOLE_NUMBER};

  for (size_t i = 0; i < arraysize(fieldsToCheckFor); ++i) {
    if (IsSectionInputUsedInFormStructure(autofill::SECTION_BILLING,
                                          fieldsToCheckFor[i], *structure)) {
      return true;
    }
  }

  return false;
}

// Return empty info string for fill fields method.
base::string16 ReturnEmptyInfo(const autofill::AutofillType& type) {
  return base::string16();
}

// Returns true if one of the nodes in |structure| request information related
// to a shipping address. To determine this actually attempt to fill the form
// using an empty data model that tracks which fields are requested.
bool RequestShippingAddress(autofill::FormStructure* structure) {
  // Country code is unused for iOS and Android, so it
  // doesn't matter what's passed.
  std::string country_code;
  autofill::DetailInputs inputs;
  // TODO(crbug.com/371074): Clean up kShippingInputs definition, unify with
  // android codebase.
  const autofill::DetailInput kShippingInputs[] = {
      {autofill::DetailInput::LONG, autofill::NAME_FULL},
      {autofill::DetailInput::LONG, autofill::ADDRESS_HOME_LINE1},
      {autofill::DetailInput::LONG, autofill::ADDRESS_HOME_LINE2},
      {autofill::DetailInput::LONG, autofill::ADDRESS_HOME_CITY},
      {autofill::DetailInput::SHORT,
       autofill::ADDRESS_HOME_STATE,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_STATE)},
      {autofill::DetailInput::SHORT_EOL, autofill::ADDRESS_HOME_ZIP},
      {autofill::DetailInput::NONE, autofill::ADDRESS_HOME_COUNTRY},
  };
  autofill::BuildInputs(kShippingInputs, arraysize(kShippingInputs), &inputs);

  // TODO(ios): [Merge r284576]: The 4th argument to FillFields()
  // is the language code based on either the billing or shipping address.
  // See implementation in upstream's autofill_dialog_controller_impl.cc
  // AutofillDialogControllerImpl::MutableAddressLanguageCodeForSection()
  // Temporarily using std::string here to complete merge.
  // See http://crbug/363063.
  return structure->FillFields(
      autofill::TypesFromInputs(inputs),
      base::Bind(autofill::ServerTypeMatchesField, autofill::SECTION_SHIPPING),
      base::Bind(&ReturnEmptyInfo), std::string(),
      GetApplicationContext()->GetApplicationLocale());
}

// Returns true if one of the nodes in |structure| request information related
// to a phone number.
bool RequestPhoneNumber(autofill::FormStructure* structure) {
  if (IsSectionInputUsedInFormStructure(autofill::SECTION_BILLING,
                                        autofill::PHONE_BILLING_WHOLE_NUMBER,
                                        *structure)) {
    return true;
  }

  if (IsSectionInputUsedInFormStructure(autofill::SECTION_SHIPPING,
                                        autofill::PHONE_HOME_WHOLE_NUMBER,
                                        *structure)) {
    return true;
  }

  return false;
}
}
