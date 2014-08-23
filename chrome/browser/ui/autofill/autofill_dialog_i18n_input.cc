// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_i18n_input.h"

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "grit/components_strings.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace i18ninput {

namespace {

using base::UTF16ToUTF8;
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::AddressUiComponent;
using ::i18n::addressinput::Localization;

std::vector<AddressUiComponent> BuildComponents(const std::string& country_code,
                                                std::string* language_code) {
  Localization localization;
  localization.SetGetter(l10n_util::GetStringUTF8);
  std::string not_used;
  return ::i18n::addressinput::BuildComponents(
      country_code, localization, g_browser_process->GetApplicationLocale(),
      language_code == NULL ? &not_used : language_code);
}

DetailInput::Length LengthFromHint(AddressUiComponent::LengthHint hint) {
  if (hint == AddressUiComponent::HINT_SHORT)
    return DetailInput::SHORT;
  DCHECK_EQ(hint, AddressUiComponent::HINT_LONG);
  return DetailInput::LONG;
}

}  // namespace

void BuildAddressInputs(common::AddressType address_type,
                        const std::string& country_code,
                        DetailInputs* inputs,
                        std::string* language_code) {
  const std::vector<AddressUiComponent>& components(
      BuildComponents(country_code, language_code));

  const bool billing = address_type == common::ADDRESS_TYPE_BILLING;

  for (size_t i = 0; i < components.size(); ++i) {
    const AddressUiComponent& component = components[i];
    // Interactive autofill dialog does not display organization.
    if (component.field == ::i18n::addressinput::ORGANIZATION)
      continue;
    ServerFieldType server_type = i18n::TypeForField(component.field, billing);
    DetailInput::Length length = LengthFromHint(component.length_hint);
    base::string16 placeholder = base::UTF8ToUTF16(component.name);
    DetailInput input = { length, server_type, placeholder };
    inputs->push_back(input);
  }

  ServerFieldType server_type =
      billing ? ADDRESS_BILLING_COUNTRY : ADDRESS_HOME_COUNTRY;
  base::string16 placeholder_text =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_COUNTRY);
  DetailInput input = { DetailInput::LONG, server_type, placeholder_text };
  inputs->push_back(input);
}

bool CardHasCompleteAndVerifiedData(const CreditCard& card) {
  if (!card.IsVerified())
    return false;

  const ServerFieldType required_fields[] = {
      CREDIT_CARD_NUMBER,
      CREDIT_CARD_EXP_MONTH,
      CREDIT_CARD_EXP_4_DIGIT_YEAR,
  };

  for (size_t i = 0; i < arraysize(required_fields); ++i) {
    if (card.GetRawInfo(required_fields[i]).empty())
      return false;
  }

  return true;
}

bool AddressHasCompleteAndVerifiedData(const AutofillProfile& profile,
                                       const std::string& app_locale) {
  if (!profile.IsVerified())
    return false;

  if (!addressinput::HasAllRequiredFields(
          *i18n::CreateAddressDataFromAutofillProfile(profile, app_locale))) {
    return false;
  }

  const ServerFieldType more_required_fields[] = {
      NAME_FULL,
      PHONE_HOME_WHOLE_NUMBER
  };

  for (size_t i = 0; i < arraysize(more_required_fields); ++i) {
    if (profile.GetInfo(AutofillType(more_required_fields[i]),
                        app_locale).empty()) {
      return false;
    }
  }

  return true;
}

}  // namespace i18ninput
}  // namespace autofill
