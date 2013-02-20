// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/data_model_wrapper.h"

#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/form_group.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/wallet/wallet_address.h"
#include "chrome/browser/autofill/wallet/wallet_address.h"
#include "chrome/browser/autofill/wallet/wallet_items.h"
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

// AutofillDataModelWrapper

AutofillDataModelWrapper::AutofillDataModelWrapper(const FormGroup* form_group,
                                                   size_t variant)
    : form_group_(form_group),
      variant_(variant) {}

AutofillDataModelWrapper::~AutofillDataModelWrapper() {}

string16 AutofillDataModelWrapper::GetInfo(AutofillFieldType type) {
  return form_group_->GetInfo(type, AutofillCountry::ApplicationLocale());
}

gfx::Image AutofillDataModelWrapper::GetIcon() {
  return gfx::Image();
}

void AutofillDataModelWrapper::FillInputs(DetailInputs* inputs) {
  // TODO(estade): use |variant_|?
  const std::string app_locale = AutofillCountry::ApplicationLocale();
  for (size_t j = 0; j < inputs->size(); ++j) {
    (*inputs)[j].autofilled_value =
        form_group_->GetInfo((*inputs)[j].type, app_locale);
  }
}

void AutofillDataModelWrapper::FillFormField(AutofillField* field) {
  form_group_->FillFormField(*field, variant_, field);
}

// AutofillCreditCardWrapper

AutofillCreditCardWrapper::AutofillCreditCardWrapper(const CreditCard* card)
    : AutofillDataModelWrapper(card, 0),
      card_(card) {}

AutofillCreditCardWrapper::~AutofillCreditCardWrapper() {}

gfx::Image AutofillCreditCardWrapper::GetIcon() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageNamed(card_->IconResourceId());
}

string16 AutofillCreditCardWrapper::GetDisplayText() {
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
  return address_->GetInfo(type);
}

gfx::Image WalletAddressWrapper::GetIcon() {
  return gfx::Image();
}

void WalletAddressWrapper::FillInputs(DetailInputs* inputs) {
  for (size_t j = 0; j < inputs->size(); ++j) {
    (*inputs)[j].autofilled_value = address_->GetInfo((*inputs)[j].type);
  }
}

void WalletAddressWrapper::FillFormField(AutofillField* field) {
  field->value = GetInfo(field->type());
}

// WalletInstrumentWrapper

WalletInstrumentWrapper::WalletInstrumentWrapper(
    const wallet::WalletItems::MaskedInstrument* instrument)
    : instrument_(instrument) {}

WalletInstrumentWrapper::~WalletInstrumentWrapper() {}

string16 WalletInstrumentWrapper::GetInfo(AutofillFieldType type) {
  // TODO(estade): implement.
  return string16(ASCIIToUTF16("foo"));
}

gfx::Image WalletInstrumentWrapper::GetIcon() {
  // TODO(estade): implement.
  return gfx::Image();
}

void WalletInstrumentWrapper::FillInputs(DetailInputs* inputs) {
  // TODO(estade): implement.
}

string16 WalletInstrumentWrapper::GetDisplayText() {
  // TODO(estade): implement better.
  return instrument_->descriptive_name();
}

void WalletInstrumentWrapper::FillFormField(AutofillField* field) {
  field->value = GetInfo(field->type());
}

}  // namespace autofill
