// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card_field.h"

#include "base/string16.h"
#include "chrome/browser/autofill/autofill_field.h"

bool CreditCardField::GetFieldInfo(FieldTypeMap* field_type_map) const {
  return false;
}

// static
CreditCardField* CreditCardField::Parse(
    std::vector<AutoFillField*>::const_iterator* iter,
    bool is_ecml) {
  CreditCardField credit_card_field;
  std::vector<AutoFillField*>::const_iterator q = *iter;
  string16 pattern;

  // Credit card fields can appear in many different orders.
  for (int fields = 0; q != *iter; ++fields) {
    // Sometimes the cardholder field is just labeled "name" (e.g. on test page
    // Starbucks - Credit card.html).  Unfortunately this is a dangerously
    // generic word to search for, since it will often match a name (not
    // cardholder name) field before or after credit card fields.  So we search
    // for "name" only when we've already parsed at least one other credit card
    // field and haven't yet parsed the expiration date (which usually appears
    // at the end).
    if (credit_card_field.cardholder_ == NULL) {
      string16 name_pattern;
      if (is_ecml) {
        name_pattern = GetEcmlPattern(kEcmlCardHolder);
      } else {
        if (fields == 0 || credit_card_field.expiration_month_) {
          // at beginning or end
          name_pattern = ASCIIToUTF16("card holder|name on card");
        } else {
          name_pattern = ASCIIToUTF16("name");
        }
      }

      if (ParseText(&q, name_pattern, &credit_card_field.cardholder_)) {
        continue;
      }

      // As a hard-coded hack for Expedia's billing pages (expedia_checkout.html
      // and ExpediaBilling.html in our test suite), recognize separate fields
      // for the cardholder's first and last name if they have the labels "cfnm"
      // and "clnm".
      std::vector<AutoFillField*>::const_iterator p = q;
      AutoFillField* first;
      if (!is_ecml && ParseText(&p, ASCIIToUTF16("^cfnm"), &first) &&
          ParseText(&p, ASCIIToUTF16("^clnm"),
                    &credit_card_field.cardholder_last_)) {
        credit_card_field.cardholder_ = first;
        q = p;
        continue;
      }
    }

    // TODO(jhawkins): Parse the type select control.

    // We look for a card security code before we look for a credit card number
    // and match the general term "number".  The security code has a plethora of
    // names; we've seen "verification #", "verification number",
    // "card identification number" and others listed in the ParseText() call
    // below.
    if (is_ecml) {
      pattern = GetEcmlPattern(kEcmlCardVerification);
    } else {
      pattern = ASCIIToUTF16(
          "verification|card identification|cvn|security code|cvv code|cvc");
    }

    if (credit_card_field.verification_ == NULL &&
        ParseText(&q, pattern, &credit_card_field.verification_)) {
      continue;
    }

    if (is_ecml)
      pattern = GetEcmlPattern(kEcmlCardNumber);
    else
      pattern = ASCIIToUTF16("number|card #|card no.|card_number");

    if (credit_card_field.number_ == NULL && ParseText(&q, pattern,
        &credit_card_field.number_)) {
      continue;
    }

    // "Expiration date" is the most common label here, but some pages have
    // "Expires", "exp. date" or "exp. month" and "exp. year".  We also look for
    // the field names ccmonth and ccyear, which appear on at least 4 of our
    // test pages.
    //
    // -> On at least one page (The China Shop2.html) we find only the labels
    // "month" and "year".  So for now we match these words directly; we'll
    // see if this turns out to be too general.
    //
    // Toolbar Bug 51451: indeed, simply matching "month" is too general for
    //   https://rps.fidelity.com/ftgw/rps/RtlCust/CreatePIN/Init.
    // Instead, we match only words beginning with "month".
    if (is_ecml)
      pattern = GetEcmlPattern(kEcmlCardExpireMonth);
    else
      pattern = ASCIIToUTF16("expir|exp month|exp date|ccmonth|&month");

    if (!credit_card_field.expiration_month_ ||
        credit_card_field.expiration_month_->IsEmpty() &&
        ParseText(&q, pattern, &credit_card_field.expiration_month_)) {
      if (is_ecml)
        pattern = GetEcmlPattern(kEcmlCardExpireYear);
      else
        pattern = ASCIIToUTF16("|exp|^/|ccyear|year");

      if (!ParseText(&q, pattern, &credit_card_field.expiration_year_)) {
        return NULL;
      }

      continue;
    }

    if (ParseText(&q, GetEcmlPattern(kEcmlCardExpireDay)))
      continue;

    // Some pages (e.g. ExpediaBilling.html) have a "card description"
    // field; we parse this field but ignore it.
    ParseText(&q, ASCIIToUTF16("card description"));
  }

  // On some pages, the user selects a card type using radio buttons
  // (e.g. test page Apple Store Billing.html).  We can't handle that yet,
  // so we treat the card type as optional for now.
  if (credit_card_field.number_ != NULL &&
      credit_card_field.expiration_month_ &&
      !credit_card_field.expiration_month_->IsEmpty()) {
      *iter = q;
      return new CreditCardField(credit_card_field);
  }

  return NULL;
}

CreditCardField::CreditCardField()
    : cardholder_(NULL),
      cardholder_last_(NULL),
      type_(NULL),
      number_(NULL),
      verification_(NULL),
      expiration_month_(NULL),
      expiration_year_(NULL) {
}

CreditCardField::CreditCardField(const CreditCardField& field)
    : cardholder_(field.cardholder_),
      cardholder_last_(field.cardholder_last_),
      type_(field.type_),
      number_(field.number_),
      verification_(field.verification_),
      expiration_month_(field.expiration_month_),
      expiration_year_(field.expiration_year_) {
}
