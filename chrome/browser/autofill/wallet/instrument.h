// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_INSTRUMENT_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_INSTRUMENT_H_

#include <string>
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"

namespace base {
class DictionaryValue;
}

namespace wallet {

class Address;

// This class contains all the data necessary to save a new instrument to a
// user's Google Wallet using WalletClient::SaveInstrument or
// WalletClient::SaveInstrumentAndAddress.
class Instrument {
 public:
  enum FormOfPayment {
    UNKNOWN,
    VISA,
    MASTER_CARD,
    AMEX,
    DISCOVER,
    JCB,
  };

  Instrument(const string16& primary_account_number,
             const string16& card_verification_number,
             int expiration_month,
             int expiration_year,
             FormOfPayment form_of_payment,
             scoped_ptr<Address> address);
  ~Instrument();

  scoped_ptr<base::DictionaryValue> ToDictionary() const;

  // Users of this class should call IsValid to check that the inputs provided
  // in the constructor were valid for use with Google Wallet.
  bool IsValid() const;

  const string16& primary_account_number() const {
    return primary_account_number_;
  }
  const string16& card_verification_number() const {
    return card_verification_number_;
  }
  int expiration_month() const { return expiration_month_; }
  int expiration_year() const { return expiration_year_; }
  const Address& address() const { return *address_; }
  FormOfPayment form_of_payment() const { return form_of_payment_; }
  const string16& last_four_digits() { return last_four_digits_; }

 private:
  // |primary_account_number_| is expected to be \d{12-19}.
  string16 primary_account_number_;

  // |card_verification_number_| is expected to be \d{3-4}.
  string16 card_verification_number_;

  // |expiration month_| should be 1-12.
  int expiration_month_;

  // |expiration_year_| should be a 4-digit year.
  int expiration_year_;

  // The payment network of the instrument, e.g. Visa.
  FormOfPayment form_of_payment_;

  // The billing address of the instrument.
  scoped_ptr<Address> address_;

  // The last four digits of |primary_account_number_|.
  string16 last_four_digits_;

  DISALLOW_COPY_AND_ASSIGN(Instrument);
};

}  // namespace wallet

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_INSTRUMENT_H_

