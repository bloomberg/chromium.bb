// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/basic_card_response.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/webpayments-methods-card/#basiccardresponse
static const char kCardBillingAddress[] = "billingAddress";
static const char kCardCardholderName[] = "cardholderName";
static const char kCardCardNumber[] = "cardNumber";
static const char kCardCardSecurityCode[] = "cardSecurityCode";
static const char kCardExpiryMonth[] = "expiryMonth";
static const char kCardExpiryYear[] = "expiryYear";

}  // namespace

BasicCardResponse::BasicCardResponse() {}
BasicCardResponse::BasicCardResponse(const BasicCardResponse& other) = default;
BasicCardResponse::~BasicCardResponse() = default;

bool BasicCardResponse::operator==(const BasicCardResponse& other) const {
  return this->cardholder_name == other.cardholder_name &&
         this->card_number == other.card_number &&
         this->expiry_month == other.expiry_month &&
         this->expiry_year == other.expiry_year &&
         this->card_security_code == other.card_security_code &&
         this->billing_address == other.billing_address;
}

bool BasicCardResponse::operator!=(const BasicCardResponse& other) const {
  return !(*this == other);
}

std::unique_ptr<base::DictionaryValue> BasicCardResponse::ToDictionaryValue()
    const {
  std::unique_ptr<base::DictionaryValue> result =
      base::MakeUnique<base::DictionaryValue>();

  result->SetString(kCardCardholderName, this->cardholder_name);
  result->SetString(kCardCardNumber, this->card_number);
  result->SetString(kCardExpiryMonth, this->expiry_month);
  result->SetString(kCardExpiryYear, this->expiry_year);
  result->SetString(kCardCardSecurityCode, this->card_security_code);
  result->Set(kCardBillingAddress, this->billing_address.ToDictionaryValue());

  return result;
}

}  // namespace payments
