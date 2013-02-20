// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/instrument.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/validation.h"
#include "chrome/browser/autofill/wallet/wallet_address.h"

namespace {

std::string FormOfPaymentToString(
    wallet::Instrument::FormOfPayment form_of_payment) {
  switch (form_of_payment) {
    case wallet::Instrument::UNKNOWN:
      return "UNKNOWN";
    case wallet::Instrument::VISA:
      return "VISA";
    case wallet::Instrument::MASTER_CARD:
      return "MASTER_CARD";
    case wallet::Instrument::AMEX:
      return "AMEX";
    case wallet::Instrument::DISCOVER:
      return "DISCOVER";
    case wallet::Instrument::JCB:
      return "JCB";
  }
  NOTREACHED();
  return "NOT_POSSIBLE";
}

}  // namespace

namespace wallet {

Instrument::Instrument(const string16& primary_account_number,
                       const string16& card_verification_number,
                       int expiration_month,
                       int expiration_year,
                       FormOfPayment form_of_payment,
                       scoped_ptr<Address> address)
    : primary_account_number_(primary_account_number),
      card_verification_number_(card_verification_number),
      expiration_month_(expiration_month),
      expiration_year_(expiration_year),
      form_of_payment_(form_of_payment),
      address_(address.Pass()) {
  DCHECK(address_);
  if (primary_account_number_.size() >=4) {
    last_four_digits_ =
        primary_account_number_.substr(primary_account_number_.size() - 4);
  }
}

Instrument::~Instrument() {}

scoped_ptr<base::DictionaryValue> Instrument::ToDictionary() const {
  // |primary_account_number_| and |card_verification_number_| can never be
  // sent the server in way that would require putting them into a dictionary.
  // Never add them to this function.

  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("type", "CREDIT_CARD");
  dict->SetInteger("credit_card.exp_month", expiration_month_);
  dict->SetInteger("credit_card.exp_year", expiration_year_);
  dict->SetString("credit_card.fop_type",
                  FormOfPaymentToString(form_of_payment_));
  dict->SetString("credit_card.last_4_digits", last_four_digits_);
  dict->Set("credit_card.address",
            address_.get()->ToDictionaryWithoutID().release());

  return dict.Pass();
}

bool Instrument::IsValid() const {
  if (!IsStringASCII(primary_account_number_))
    return false;
  bool primary_account_number_valid =
      autofill::IsValidCreditCardNumber(primary_account_number_);
  bool card_verification_number_valid = card_verification_number_.size() == 3 ||
                                        card_verification_number_.size() == 4;
  bool exp_month_valid = expiration_month_ >= 1 && expiration_month_ <= 12;
  bool exp_year_valid = expiration_year_ >= 2013 && expiration_year_ <= 2100;

  return primary_account_number_valid &&
         card_verification_number_valid &&
         exp_month_valid &&
         exp_year_valid;
}

}  // namespace wallet
