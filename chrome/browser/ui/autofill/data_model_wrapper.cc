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

void DataModelWrapper::FillInputs(DetailInputs* inputs) {
  for (size_t i = 0; i < inputs->size(); ++i) {
    (*inputs)[i].initial_value = GetInfo((*inputs)[i].type);
  }
}

void DataModelWrapper::FillFormField(AutofillField* field) const {
  field->value = GetInfo(field->type());
}

DataModelWrapper::DataModelWrapper() {}

gfx::Image DataModelWrapper::GetIcon() {
  return gfx::Image();
}

// EmptyDataModelWrapper

EmptyDataModelWrapper::EmptyDataModelWrapper() {}
EmptyDataModelWrapper::~EmptyDataModelWrapper() {}

string16 EmptyDataModelWrapper::GetInfo(AutofillFieldType type) const {
  return string16();
}

void EmptyDataModelWrapper::FillFormField(AutofillField* field) const {}

// AutofillDataModelWrapper

AutofillDataModelWrapper::AutofillDataModelWrapper(
    const AutofillDataModel* data_model,
    size_t variant)
    : data_model_(data_model),
      variant_(variant) {}

AutofillDataModelWrapper::~AutofillDataModelWrapper() {}

string16 AutofillDataModelWrapper::GetInfo(AutofillFieldType type) const {
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

string16 AutofillCreditCardWrapper::GetInfo(AutofillFieldType type) const {
  if (type == CREDIT_CARD_EXP_MONTH)
    return MonthComboboxModel::FormatMonth(card_->expiration_month());

  return AutofillDataModelWrapper::GetInfo(type);
}

gfx::Image AutofillCreditCardWrapper::GetIcon() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageNamed(CreditCard::IconResourceId(card_->type()));
}

string16 AutofillCreditCardWrapper::GetDisplayText() {
  if (!card_->IsValid())
    return string16();

  return card_->TypeAndLastFourDigits();
}

void AutofillCreditCardWrapper::FillFormField(AutofillField* field) const {
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

string16 WalletAddressWrapper::GetInfo(AutofillFieldType type) const {
  return address_->GetInfo(type, g_browser_process->GetApplicationLocale());
}

string16 WalletAddressWrapper::GetDisplayText() {
  if (!address_->is_complete_address() ||
      GetInfo(PHONE_HOME_WHOLE_NUMBER).empty()) {
    return string16();
  }

  return DataModelWrapper::GetDisplayText();
}

// WalletInstrumentWrapper

WalletInstrumentWrapper::WalletInstrumentWrapper(
    const wallet::WalletItems::MaskedInstrument* instrument)
    : instrument_(instrument) {}

WalletInstrumentWrapper::~WalletInstrumentWrapper() {}

string16 WalletInstrumentWrapper::GetInfo(AutofillFieldType type) const {
  if (type == CREDIT_CARD_EXP_MONTH)
    return MonthComboboxModel::FormatMonth(instrument_->expiration_month());

  return instrument_->GetInfo(type, g_browser_process->GetApplicationLocale());
}

gfx::Image WalletInstrumentWrapper::GetIcon() {
  return instrument_->CardIcon();
}

string16 WalletInstrumentWrapper::GetDisplayText() {
  // TODO(dbeam): handle other instrument statuses? http://crbug.com/233048
  if (instrument_->status() == wallet::WalletItems::MaskedInstrument::EXPIRED ||
      !instrument_->address().is_complete_address() ||
      GetInfo(PHONE_HOME_WHOLE_NUMBER).empty()) {
    return string16();
  }

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

string16 FullWalletBillingWrapper::GetInfo(AutofillFieldType type) const {
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

string16 FullWalletShippingWrapper::GetInfo(AutofillFieldType type) const {
  return full_wallet_->shipping_address()->GetInfo(
      type, g_browser_process->GetApplicationLocale());
}

}  // namespace autofill
