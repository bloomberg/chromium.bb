// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/data_model_wrapper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/autofill_dialog_i18n_input.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace autofill {

DataModelWrapper::~DataModelWrapper() {}

void DataModelWrapper::FillInputs(DetailInputs* inputs) {
  for (size_t i = 0; i < inputs->size(); ++i) {
    (*inputs)[i].initial_value =
        GetInfoForDisplay(AutofillType((*inputs)[i].type));
  }
}

base::string16 DataModelWrapper::GetInfoForDisplay(const AutofillType& type)
    const {
  return GetInfo(type);
}

gfx::Image DataModelWrapper::GetIcon() {
  return gfx::Image();
}

bool DataModelWrapper::GetDisplayText(
    base::string16* vertically_compact,
    base::string16* horizontally_compact) {
  base::string16 phone =
      GetInfoForDisplay(AutofillType(PHONE_HOME_WHOLE_NUMBER));
  if (phone.empty())
    return false;

  // Format the address.
  scoped_ptr< ::i18n::addressinput::AddressData> address_data =
      i18n::CreateAddressData(
          base::Bind(&DataModelWrapper::GetInfo, base::Unretained(this)));
  address_data->language_code = GetLanguageCode();
  // Interactive autofill dialog does not display organization field.
  address_data->organization.clear();
  std::vector<std::string> lines;
  ::i18n::addressinput::GetFormattedNationalAddress(*address_data, &lines);

  std::string single_line;
  ::i18n::addressinput::GetFormattedNationalAddressLine(
       *address_data, &single_line);

  // Email and phone number aren't part of address formatting.
  base::string16 non_address_info;
  base::string16 email = GetInfoForDisplay(AutofillType(EMAIL_ADDRESS));
  if (!email.empty())
    non_address_info += base::ASCIIToUTF16("\n") + email;

  non_address_info += base::ASCIIToUTF16("\n") + phone;

  *vertically_compact = base::UTF8ToUTF16(single_line) + non_address_info;
  *horizontally_compact = base::UTF8ToUTF16(base::JoinString(lines, "\n")) +
      non_address_info;

  return true;
}

bool DataModelWrapper::FillFormStructure(
    const std::vector<ServerFieldType>& types,
    const FormStructure::InputFieldComparator& compare,
    FormStructure* form_structure) const {
  return form_structure->FillFields(
      types,
      compare,
      base::Bind(&DataModelWrapper::GetInfo, base::Unretained(this)),
      GetLanguageCode(),
      g_browser_process->GetApplicationLocale());
}

DataModelWrapper::DataModelWrapper() {}

// AutofillProfileWrapper

AutofillProfileWrapper::AutofillProfileWrapper(const AutofillProfile* profile)
    : profile_(profile) {}

AutofillProfileWrapper::~AutofillProfileWrapper() {}

base::string16 AutofillProfileWrapper::GetInfo(const AutofillType& type) const {
  // Requests for the user's credit card are filled from the billing address,
  // but the AutofillProfile class doesn't know how to fill credit card
  // fields. So, request for the corresponding profile type instead.
  AutofillType effective_type = type;
  if (type.GetStorableType() == CREDIT_CARD_NAME)
    effective_type = AutofillType(NAME_BILLING_FULL);

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  return profile_->GetInfo(effective_type, app_locale);
}

base::string16 AutofillProfileWrapper::GetInfoForDisplay(
    const AutofillType& type) const {
  // We display the "raw" phone number which contains user-defined formatting.
  if (type.GetStorableType() == PHONE_HOME_WHOLE_NUMBER) {
    base::string16 phone_number = profile_->GetRawInfo(type.GetStorableType());

    // If there is no user-defined formatting at all, add some standard
    // formatting.
    if (base::ContainsOnlyChars(phone_number,
                                base::ASCIIToUTF16("+0123456789"))) {
      std::string region = base::UTF16ToASCII(
          GetInfo(AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE)));
      i18n::PhoneObject phone(phone_number, region);
      base::string16 formatted_number = phone.GetFormattedNumber();
      // Formatting may fail.
      if (!formatted_number.empty())
        return formatted_number;
    }

    return phone_number;
  }

  return DataModelWrapper::GetInfoForDisplay(type);
}

const std::string& AutofillProfileWrapper::GetLanguageCode() const {
  return profile_->language_code();
}

// AutofillShippingAddressWrapper

AutofillShippingAddressWrapper::AutofillShippingAddressWrapper(
    const AutofillProfile* profile)
    : AutofillProfileWrapper(profile) {}

AutofillShippingAddressWrapper::~AutofillShippingAddressWrapper() {}

base::string16 AutofillShippingAddressWrapper::GetInfo(
    const AutofillType& type) const {
  // Shipping addresses don't have email addresses associated with them.
  if (type.GetStorableType() == EMAIL_ADDRESS)
    return base::string16();

  return AutofillProfileWrapper::GetInfo(type);
}

// AutofillCreditCardWrapper

AutofillCreditCardWrapper::AutofillCreditCardWrapper(const CreditCard* card)
    : card_(card) {}

AutofillCreditCardWrapper::~AutofillCreditCardWrapper() {}

base::string16 AutofillCreditCardWrapper::GetInfo(const AutofillType& type)
    const {
  if (type.group() != CREDIT_CARD)
    return base::string16();

  if (type.GetStorableType() == CREDIT_CARD_EXP_MONTH)
    return MonthComboboxModel::FormatMonth(card_->expiration_month());

  return card_->GetInfo(type, g_browser_process->GetApplicationLocale());
}

gfx::Image AutofillCreditCardWrapper::GetIcon() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageNamed(CreditCard::IconResourceId(card_->type()));
}

bool AutofillCreditCardWrapper::GetDisplayText(
    base::string16* vertically_compact,
    base::string16* horizontally_compact) {
  if (!card_->IsValid())
    return false;

  *vertically_compact = *horizontally_compact = card_->TypeAndLastFourDigits();
  return true;
}

const std::string& AutofillCreditCardWrapper::GetLanguageCode() const {
  // Formatting a credit card for display does not depend on language code.
  return base::EmptyString();
}

// I18nAddressDataWrapper

I18nAddressDataWrapper::I18nAddressDataWrapper(
    const ::i18n::addressinput::AddressData* address)
    : address_(address) {}

I18nAddressDataWrapper::~I18nAddressDataWrapper() {}

base::string16 I18nAddressDataWrapper::GetInfo(const AutofillType& type) const {
  ::i18n::addressinput::AddressField field;
  if (!i18n::FieldForType(type.GetStorableType(), &field))
    return base::string16();

  if (field == ::i18n::addressinput::STREET_ADDRESS)
    return base::string16();

  if (field == ::i18n::addressinput::COUNTRY) {
    return AutofillCountry(address_->region_code,
                           g_browser_process->GetApplicationLocale()).name();
  }

  return base::UTF8ToUTF16(address_->GetFieldValue(field));
}

const std::string& I18nAddressDataWrapper::GetLanguageCode() const {
  return address_->language_code;
}

}  // namespace autofill
