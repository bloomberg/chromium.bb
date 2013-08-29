// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/data_model_wrapper.h"

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "components/autofill/content/browser/wallet/full_wallet.h"
#include "components/autofill/content/browser/wallet/wallet_address.h"
#include "components/autofill/content/browser/wallet/wallet_items.h"
#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/validation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace autofill {

DataModelWrapper::~DataModelWrapper() {}

gfx::Image DataModelWrapper::GetIcon() {
  return gfx::Image();
}

void DataModelWrapper::FillInputs(DetailInputs* inputs) {
  for (size_t i = 0; i < inputs->size(); ++i) {
    (*inputs)[i].initial_value = GetInfo(AutofillType((*inputs)[i].type));
  }
}

bool DataModelWrapper::GetDisplayText(
    base::string16* vertically_compact,
    base::string16* horizontally_compact) {
  base::string16 comma = ASCIIToUTF16(", ");
  base::string16 newline = ASCIIToUTF16("\n");

  *vertically_compact = GetAddressDisplayText(comma);
  *horizontally_compact = GetAddressDisplayText(newline);
  return true;
}

bool DataModelWrapper::FillFormStructure(
    const DetailInputs& inputs,
    const InputFieldComparator& compare,
    FormStructure* form_structure) const {
  bool filled_something = false;
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    AutofillField* field = form_structure->field(i);
    for (size_t j = 0; j < inputs.size(); ++j) {
      if (compare.Run(inputs[j], *field)) {
        FillFormField(field);
        filled_something = true;
        break;
      }
    }
  }
  return filled_something;
}

DataModelWrapper::DataModelWrapper() {}

void DataModelWrapper::FillFormField(AutofillField* field) const {
  field->value = GetInfo(field->Type());
}

base::string16 DataModelWrapper::GetAddressDisplayText(
    const base::string16& separator) {
  base::string16 address = GetInfo(AutofillType(NAME_FULL)) + separator +
      GetInfo(AutofillType(ADDRESS_HOME_LINE1));
  base::string16 address2 = GetInfo(AutofillType(ADDRESS_HOME_LINE2));
  if (!address2.empty())
    address += separator + address2;

  base::string16 comma = ASCIIToUTF16(", ");
  base::string16 newline = ASCIIToUTF16("\n");
  address += separator +
      GetInfo(AutofillType(ADDRESS_HOME_CITY)) + comma +
      GetInfo(AutofillType(ADDRESS_HOME_STATE)) + ASCIIToUTF16(" ") +
      GetInfo(AutofillType(ADDRESS_HOME_ZIP));

  // TODO(estade): email?
  address += newline + GetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER));

  return address;
}

// EmptyDataModelWrapper

EmptyDataModelWrapper::EmptyDataModelWrapper() {}
EmptyDataModelWrapper::~EmptyDataModelWrapper() {}

base::string16 EmptyDataModelWrapper::GetInfo(const AutofillType& type) const {
  return base::string16();
}

void EmptyDataModelWrapper::FillFormField(AutofillField* field) const {}

// AutofillDataModelWrapper

AutofillDataModelWrapper::AutofillDataModelWrapper(
    const AutofillDataModel* data_model,
    size_t variant)
    : data_model_(data_model),
      variant_(variant) {}

AutofillDataModelWrapper::~AutofillDataModelWrapper() {}

base::string16 AutofillDataModelWrapper::GetInfo(const AutofillType& type)
    const {
  return data_model_->GetInfo(type, g_browser_process->GetApplicationLocale());
}

void AutofillDataModelWrapper::FillFormField(AutofillField* field) const {
  data_model_->FillFormField(
      *field, variant_, g_browser_process->GetApplicationLocale(), field);
}

// AutofillProfileWrapper

AutofillProfileWrapper::AutofillProfileWrapper(
    const AutofillProfile* profile, size_t variant)
    : AutofillDataModelWrapper(profile, variant),
      profile_(profile) {}

AutofillProfileWrapper::~AutofillProfileWrapper() {}

void AutofillProfileWrapper::FillInputs(DetailInputs* inputs) {
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  for (size_t j = 0; j < inputs->size(); ++j) {
    std::vector<base::string16> values;
    profile_->GetMultiInfo(
        AutofillType((*inputs)[j].type), app_locale, &values);
    (*inputs)[j].initial_value = values[variant()];
  }
}

void AutofillProfileWrapper::FillFormField(AutofillField* field) const {
  if (field->Type().GetStorableType() == CREDIT_CARD_NAME) {
    // Cache the field's true type.
    HtmlFieldType original_type = field->html_type();

    // Requests for the user's credit card are filled from the billing address,
    // but the AutofillProfile class doesn't know how to fill credit card
    // fields. So, temporarily set the type to the corresponding profile type.
    field->SetHtmlType(HTML_TYPE_NAME, field->html_mode());
    AutofillDataModelWrapper::FillFormField(field);

    // Restore the field's true type.
    field->SetHtmlType(original_type, field->html_mode());
  } else {
    AutofillDataModelWrapper::FillFormField(field);
  }
}

// AutofillCreditCardWrapper

AutofillCreditCardWrapper::AutofillCreditCardWrapper(const CreditCard* card)
    : AutofillDataModelWrapper(card, 0),
      card_(card) {}

AutofillCreditCardWrapper::~AutofillCreditCardWrapper() {}

base::string16 AutofillCreditCardWrapper::GetInfo(const AutofillType& type)
    const {
  if (type.group() != CREDIT_CARD)
    return base::string16();

  if (type.GetStorableType() == CREDIT_CARD_EXP_MONTH)
    return MonthComboboxModel::FormatMonth(card_->expiration_month());

  return AutofillDataModelWrapper::GetInfo(type);
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

// WalletAddressWrapper

WalletAddressWrapper::WalletAddressWrapper(
    const wallet::Address* address) : address_(address) {}

WalletAddressWrapper::~WalletAddressWrapper() {}

base::string16 WalletAddressWrapper::GetInfo(const AutofillType& type) const {
  return address_->GetInfo(type, g_browser_process->GetApplicationLocale());
}

bool WalletAddressWrapper::GetDisplayText(
    base::string16* vertically_compact,
    base::string16* horizontally_compact) {
  if (!address_->is_complete_address() ||
      GetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER)).empty()) {
    return false;
  }

  return DataModelWrapper::GetDisplayText(vertically_compact,
                                          horizontally_compact);
}

// WalletInstrumentWrapper

WalletInstrumentWrapper::WalletInstrumentWrapper(
    const wallet::WalletItems::MaskedInstrument* instrument)
    : instrument_(instrument) {}

WalletInstrumentWrapper::~WalletInstrumentWrapper() {}

base::string16 WalletInstrumentWrapper::GetInfo(const AutofillType& type)
    const {
  if (type.GetStorableType() == CREDIT_CARD_EXP_MONTH)
    return MonthComboboxModel::FormatMonth(instrument_->expiration_month());

  return instrument_->GetInfo(type, g_browser_process->GetApplicationLocale());
}

gfx::Image WalletInstrumentWrapper::GetIcon() {
  return instrument_->CardIcon();
}

bool WalletInstrumentWrapper::GetDisplayText(
    base::string16* vertically_compact,
    base::string16* horizontally_compact) {
  // TODO(dbeam): handle other instrument statuses? http://crbug.com/233048
  if (instrument_->status() == wallet::WalletItems::MaskedInstrument::EXPIRED ||
      !instrument_->address().is_complete_address() ||
      GetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER)).empty()) {
    return false;
  }

  DataModelWrapper::GetDisplayText(vertically_compact, horizontally_compact);
  // TODO(estade): descriptive_name() is user-provided. Should we use it or
  // just type + last 4 digits?
  base::string16 line1 = instrument_->descriptive_name() + ASCIIToUTF16("\n");
  *vertically_compact = line1 + *vertically_compact;
  *horizontally_compact = line1 + *horizontally_compact;
  return true;
}

// FullWalletBillingWrapper

FullWalletBillingWrapper::FullWalletBillingWrapper(
    wallet::FullWallet* full_wallet)
    : full_wallet_(full_wallet) {
  DCHECK(full_wallet_);
}

FullWalletBillingWrapper::~FullWalletBillingWrapper() {}

base::string16 FullWalletBillingWrapper::GetInfo(const AutofillType& type)
    const {
  if (type.GetStorableType() == CREDIT_CARD_EXP_MONTH)
    return MonthComboboxModel::FormatMonth(full_wallet_->expiration_month());

  if (type.group() == CREDIT_CARD)
    return full_wallet_->GetInfo(type);

  return full_wallet_->billing_address()->GetInfo(
      type, g_browser_process->GetApplicationLocale());
}

bool FullWalletBillingWrapper::GetDisplayText(
    base::string16* vertically_compact,
    base::string16* horizontally_compact) {
  // TODO(dbeam): handle other required actions? http://crbug.com/163508
  if (full_wallet_->HasRequiredAction(wallet::UPDATE_EXPIRATION_DATE))
    return false;

  return DataModelWrapper::GetDisplayText(vertically_compact,
                                          horizontally_compact);
}

// FullWalletShippingWrapper

FullWalletShippingWrapper::FullWalletShippingWrapper(
    wallet::FullWallet* full_wallet)
    : full_wallet_(full_wallet) {
  DCHECK(full_wallet_);
}

FullWalletShippingWrapper::~FullWalletShippingWrapper() {}

base::string16 FullWalletShippingWrapper::GetInfo(
    const AutofillType& type) const {
  return full_wallet_->shipping_address()->GetInfo(
      type, g_browser_process->GetApplicationLocale());
}

DetailOutputWrapper::DetailOutputWrapper(const DetailOutputMap& outputs)
    : outputs_(outputs) {}

DetailOutputWrapper::~DetailOutputWrapper() {}

base::string16 DetailOutputWrapper::GetInfo(const AutofillType& type) const {
  ServerFieldType storable_type = type.GetStorableType();
  for (DetailOutputMap::const_iterator it = outputs_.begin();
       it != outputs_.end(); ++it) {
    if (storable_type == AutofillType(it->first->type).GetStorableType())
      return it->second;
  }
  return base::string16();
}

}  // namespace autofill
