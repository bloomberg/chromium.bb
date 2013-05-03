// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/data_model_wrapper.h"

#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "components/autofill/browser/autofill_data_model.h"
#include "components/autofill/browser/autofill_profile.h"
#include "components/autofill/browser/autofill_type.h"
#include "components/autofill/browser/credit_card.h"
#include "components/autofill/browser/form_structure.h"
#include "components/autofill/browser/validation.h"
#include "components/autofill/browser/wallet/full_wallet.h"
#include "components/autofill/browser/wallet/wallet_address.h"
#include "components/autofill/browser/wallet/wallet_items.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace autofill {

DataModelWrapper::~DataModelWrapper() {}

string16 DataModelWrapper::GetDisplayText() {
  string16 comma = ASCIIToUTF16(", ");
  string16 label = GetInfo(NAME_FULL) + comma + GetInfo(ADDRESS_HOME_LINE1);
  string16 address2 = GetInfo(ADDRESS_HOME_LINE2);
  if (!address2.empty())
    label += comma + address2;
  label += ASCIIToUTF16("\n") +
      GetInfo(ADDRESS_HOME_CITY) + comma +
      GetInfo(ADDRESS_HOME_STATE) + ASCIIToUTF16(" ") +
      GetInfo(ADDRESS_HOME_ZIP);
  return label;
}

void DataModelWrapper::FillFormStructure(
    const DetailInputs& inputs,
    const InputFieldComparator& compare,
    FormStructure* form_structure) {
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    AutofillField* field = form_structure->field(i);
    for (size_t j = 0; j < inputs.size(); ++j) {
      if (compare.Run(inputs[j], *field)) {
        FillFormField(field);
        break;
      }
    }
  }
}

void DataModelWrapper::FillInputs(DetailInputs* inputs) {
  for (size_t i = 0; i < inputs->size(); ++i) {
    (*inputs)[i].initial_value = GetInfo((*inputs)[i].type);
  }
}

void DataModelWrapper::FillFormField(AutofillField* field) {
  field->value = GetInfo(field->type());
}

DataModelWrapper::DataModelWrapper() {}

gfx::Image DataModelWrapper::GetIcon() {
  return gfx::Image();
}

// AutofillDataModelWrapper

AutofillDataModelWrapper::AutofillDataModelWrapper(
    const AutofillDataModel* data_model,
    size_t variant)
    : data_model_(data_model),
      variant_(variant) {}

AutofillDataModelWrapper::~AutofillDataModelWrapper() {}

string16 AutofillDataModelWrapper::GetInfo(AutofillFieldType type) {
  return data_model_->GetInfo(type, g_browser_process->GetApplicationLocale());
}

void AutofillDataModelWrapper::FillFormField(AutofillField* field) {
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
    std::vector<string16> values;
    profile_->GetMultiInfo((*inputs)[j].type, app_locale, &values);
    (*inputs)[j].initial_value = values[variant()];
  }
}

// AutofillCreditCardWrapper

AutofillCreditCardWrapper::AutofillCreditCardWrapper(const CreditCard* card)
    : AutofillDataModelWrapper(card, 0),
      card_(card) {}

AutofillCreditCardWrapper::~AutofillCreditCardWrapper() {}

string16 AutofillCreditCardWrapper::GetInfo(AutofillFieldType type) {
  if (type == CREDIT_CARD_EXP_MONTH)
    return MonthComboboxModel::FormatMonth(card_->expiration_month());

  return AutofillDataModelWrapper::GetInfo(type);
}

gfx::Image AutofillCreditCardWrapper::GetIcon() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageNamed(card_->IconResourceId());
}

string16 AutofillCreditCardWrapper::GetDisplayText() {
  if (!autofill::IsValidCreditCardExpirationDate(
           card_->GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR),
           card_->GetRawInfo(CREDIT_CARD_EXP_MONTH),
           base::Time::Now())) {
    return string16();
  }

  return card_->TypeAndLastFourDigits();
}

void AutofillCreditCardWrapper::FillFormField(AutofillField* field) {
  AutofillFieldType field_type = field->type();

  if (field_type == NAME_FULL) {
    // Requests for the user's full name are filled from the credit card data,
    // but the CreditCard class only knows how to fill credit card fields.  So,
    // temporarily set the type to the corresponding credit card type.
    field->set_heuristic_type(CREDIT_CARD_NAME);
  }

  AutofillDataModelWrapper::FillFormField(field);

  field->set_heuristic_type(field_type);
}

// WalletAddressWrapper

WalletAddressWrapper::WalletAddressWrapper(
    const wallet::Address* address) : address_(address) {}

WalletAddressWrapper::~WalletAddressWrapper() {}

string16 WalletAddressWrapper::GetInfo(AutofillFieldType type) {
  return address_->GetInfo(type, g_browser_process->GetApplicationLocale());
}

// WalletInstrumentWrapper

WalletInstrumentWrapper::WalletInstrumentWrapper(
    const wallet::WalletItems::MaskedInstrument* instrument)
    : instrument_(instrument) {}

WalletInstrumentWrapper::~WalletInstrumentWrapper() {}

string16 WalletInstrumentWrapper::GetInfo(AutofillFieldType type) {
  if (type == CREDIT_CARD_EXP_MONTH)
    return MonthComboboxModel::FormatMonth(instrument_->expiration_month());

  return instrument_->GetInfo(type, g_browser_process->GetApplicationLocale());
}

gfx::Image WalletInstrumentWrapper::GetIcon() {
  return instrument_->CardIcon();
}

string16 WalletInstrumentWrapper::GetDisplayText() {
  // TODO(dbeam): handle other instrument statuses? http://crbug.com/233048
  if (instrument_->status() == wallet::WalletItems::MaskedInstrument::EXPIRED)
    return string16();

  // TODO(estade): descriptive_name() is user-provided. Should we use it or
  // just type + last 4 digits?
  string16 line1 = instrument_->descriptive_name();
  return line1 + ASCIIToUTF16("\n") + DataModelWrapper::GetDisplayText();
}

// FullWalletBillingWrapper

FullWalletBillingWrapper::FullWalletBillingWrapper(
    wallet::FullWallet* full_wallet)
    : full_wallet_(full_wallet) {
  DCHECK(full_wallet_);
}

FullWalletBillingWrapper::~FullWalletBillingWrapper() {}

string16 FullWalletBillingWrapper::GetInfo(AutofillFieldType type) {
  if (AutofillType(type).group() == AutofillType::CREDIT_CARD)
    return full_wallet_->GetInfo(type);

  return full_wallet_->billing_address()->GetInfo(
      type, g_browser_process->GetApplicationLocale());
}

string16 FullWalletBillingWrapper::GetDisplayText() {
  // TODO(dbeam): handle other required actions? http://crbug.com/163508
  if (full_wallet_->HasRequiredAction(wallet::UPDATE_EXPIRATION_DATE))
    return string16();

  return DataModelWrapper::GetDisplayText();
}

// FullWalletShippingWrapper

FullWalletShippingWrapper::FullWalletShippingWrapper(
    wallet::FullWallet* full_wallet)
    : full_wallet_(full_wallet) {
  DCHECK(full_wallet_);
}

FullWalletShippingWrapper::~FullWalletShippingWrapper() {}

string16 FullWalletShippingWrapper::GetInfo(AutofillFieldType type) {
  return full_wallet_->shipping_address()->GetInfo(
      type, g_browser_process->GetApplicationLocale());
}

}  // namespace autofill
