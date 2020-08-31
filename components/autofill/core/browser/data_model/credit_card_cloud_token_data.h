// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_CLOUD_TOKEN_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_CLOUD_TOKEN_DATA_H_

#include <string>

#include "base/strings/string16.h"

namespace autofill {

// Represents all the cloud tokenization data related to the server credit card.
struct CreditCardCloudTokenData {
 public:
  CreditCardCloudTokenData();
  CreditCardCloudTokenData(const CreditCardCloudTokenData& cloud_token_data);
  ~CreditCardCloudTokenData();

  bool operator==(const CreditCardCloudTokenData&) const;
  bool operator!=(const CreditCardCloudTokenData&) const;

  base::string16 ExpirationMonthAsString() const;
  base::string16 Expiration2DigitYearAsString() const;
  base::string16 Expiration4DigitYearAsString() const;
  void SetExpirationMonthFromString(const base::string16& month);
  void SetExpirationYearFromString(const base::string16& year);

  // Used by Autofill Wallet sync bridge to compute the difference between two
  // CreditCardCloudTokenData.
  int Compare(const CreditCardCloudTokenData& cloud_token_data) const;

  // The id assigned by the server to uniquely identify this card.
  std::string masked_card_id;

  // The last 4-5 digits of the Cloud Primary Account Number (CPAN).
  base::string16 suffix;

  // The expiration month of the CPAN.
  int exp_month = 0;

  // The 4-digit expiration year of the CPAN.
  int exp_year = 0;

  // The URL of the card art to be displayed.
  std::string card_art_url;

  // The opaque identifier for the cloud token associated with the card.
  std::string instrument_token;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_CLOUD_TOKEN_DATA_H_
