// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_method_data.h"

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/browser-payment-api/#paymentmethoddata-dictionary
static const char kMethodDataData[] = "data";
static const char kSupportedMethods[] = "supportedMethods";
static const char kSupportedNetworks[] = "supportedNetworks";
static const char kSupportedTypes[] = "supportedTypes";

// Converts |supported_type| to |card_type| and returns true on success.
bool ConvertCardTypeStringToEnum(const std::string& supported_type,
                                 autofill::CreditCard::CardType* card_type) {
  if (supported_type == "credit") {
    *card_type = autofill::CreditCard::CARD_TYPE_CREDIT;
    return true;
  }

  if (supported_type == "debit") {
    *card_type = autofill::CreditCard::CARD_TYPE_DEBIT;
    return true;
  }

  if (supported_type == "prepaid") {
    *card_type = autofill::CreditCard::CARD_TYPE_PREPAID;
    return true;
  }

  return false;
}

}  // namespace

PaymentMethodData::PaymentMethodData() {}
PaymentMethodData::PaymentMethodData(const PaymentMethodData& other) = default;
PaymentMethodData::~PaymentMethodData() = default;

bool PaymentMethodData::operator==(const PaymentMethodData& other) const {
  return this->supported_methods == other.supported_methods &&
         this->data == other.data &&
         this->supported_networks == other.supported_networks &&
         this->supported_types == other.supported_types;
}

bool PaymentMethodData::operator!=(const PaymentMethodData& other) const {
  return !(*this == other);
}

bool PaymentMethodData::FromDictionaryValue(
    const base::DictionaryValue& value) {
  this->supported_methods.clear();
  this->supported_networks.clear();
  this->supported_types.clear();

  const base::ListValue* supported_methods_list = nullptr;
  // At least one supported method is required.
  if (!value.GetList(kSupportedMethods, &supported_methods_list) ||
      supported_methods_list->GetSize() == 0) {
    return false;
  }
  for (size_t i = 0; i < supported_methods_list->GetSize(); ++i) {
    std::string supported_method;
    if (!supported_methods_list->GetString(i, &supported_method) ||
        !base::IsStringASCII(supported_method)) {
      return false;
    }
    this->supported_methods.push_back(supported_method);
  }

  // Data is optional, but if a dictionary is present, save a stringified
  // version and attempt to parse supportedNetworks/supportedTypes.
  const base::DictionaryValue* data_dict = nullptr;
  if (value.GetDictionary(kMethodDataData, &data_dict)) {
    std::string json_data;
    base::JSONWriter::Write(*data_dict, &json_data);
    this->data = json_data;
    const base::ListValue* supported_networks_list = nullptr;
    if (data_dict->GetList(kSupportedNetworks, &supported_networks_list)) {
      for (size_t i = 0; i < supported_networks_list->GetSize(); ++i) {
        std::string supported_network;
        if (!supported_networks_list->GetString(i, &supported_network) ||
            !base::IsStringASCII(supported_network)) {
          return false;
        }
        this->supported_networks.push_back(supported_network);
      }
    }
    const base::ListValue* supported_types_list = nullptr;
    if (data_dict->GetList(kSupportedTypes, &supported_types_list)) {
      for (size_t i = 0; i < supported_types_list->GetSize(); ++i) {
        std::string supported_type;
        if (!supported_types_list->GetString(i, &supported_type) ||
            !base::IsStringASCII(supported_type)) {
          return false;
        }
        autofill::CreditCard::CardType card_type =
            autofill::CreditCard::CARD_TYPE_UNKNOWN;
        if (ConvertCardTypeStringToEnum(supported_type, &card_type))
          this->supported_types.insert(card_type);
      }
    }
  }
  return true;
}

}  // namespace payments
