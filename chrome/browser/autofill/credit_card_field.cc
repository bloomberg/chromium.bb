// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card_field.h"

#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "grit/autofill_resources.h"
#include "ui/base/l10n/l10n_util.h"

bool CreditCardField::GetFieldInfo(FieldTypeMap* field_type_map) const {
  bool ok = Add(field_type_map, number_, AutoFillType(CREDIT_CARD_NUMBER));
  DCHECK(ok);

  // If the heuristics detected first and last name in separate fields,
  // then ignore both fields. Putting them into separate fields is probably
  // wrong, because the credit card can also contain a middle name or middle
  // initial.
  if (cardholder_last_ == NULL) {
    // Add() will check if cardholder_ is != NULL.
    ok = ok && Add(field_type_map, cardholder_, AutoFillType(CREDIT_CARD_NAME));
    DCHECK(ok);
  }

  ok = ok && Add(field_type_map, type_, AutoFillType(CREDIT_CARD_TYPE));
  DCHECK(ok);
  ok = ok && Add(field_type_map, expiration_month_,
      AutoFillType(CREDIT_CARD_EXP_MONTH));
  DCHECK(ok);
  ok = ok && Add(field_type_map, expiration_year_,
      AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  DCHECK(ok);

  return ok;
}

FormFieldType CreditCardField::GetFormFieldType() const {
  return kCreditCardType;
}

// static
CreditCardField* CreditCardField::Parse(
    std::vector<AutoFillField*>::const_iterator* iter,
    bool is_ecml) {
  scoped_ptr<CreditCardField> credit_card_field(new CreditCardField);
  std::vector<AutoFillField*>::const_iterator q = *iter;
  string16 pattern;

  // Credit card fields can appear in many different orders.
  // We loop until no more credit card related fields are found, see |break| at
  // bottom of the loop.
  for (int fields = 0; true; ++fields) {
    // Sometimes the cardholder field is just labeled "name". Unfortunately this
    // is a dangerously generic word to search for, since it will often match a
    // name (not cardholder name) field before or after credit card fields. So
    // we search for "name" only when we've already parsed at least one other
    // credit card field and haven't yet parsed the expiration date (which
    // usually appears at the end).
    if (credit_card_field->cardholder_ == NULL) {
      string16 name_pattern;
      if (is_ecml) {
        name_pattern = GetEcmlPattern(kEcmlCardHolder);
      } else {
        if (fields == 0 || credit_card_field->expiration_month_) {
          // at beginning or end
          name_pattern = l10n_util::GetStringUTF16(
              IDS_AUTOFILL_NAME_ON_CARD_RE);
        } else {
          name_pattern = l10n_util::GetStringUTF16(
              IDS_AUTOFILL_NAME_ON_CARD_CONTEXTUAL_RE);
        }
      }

      if (ParseText(&q, name_pattern, &credit_card_field->cardholder_))
        continue;

      // As a hard-coded hack for Expedia's billing pages (expedia_checkout.html
      // and ExpediaBilling.html in our test suite), recognize separate fields
      // for the cardholder's first and last name if they have the labels "cfnm"
      // and "clnm".
      std::vector<AutoFillField*>::const_iterator p = q;
      AutoFillField* first;
      if (!is_ecml && ParseText(&p, ASCIIToUTF16("^cfnm"), &first) &&
          ParseText(&p, ASCIIToUTF16("^clnm"),
                    &credit_card_field->cardholder_last_)) {
        credit_card_field->cardholder_ = first;
        q = p;
        continue;
      }
    }

    // We look for a card security code before we look for a credit
    // card number and match the general term "number".  The security code
    // has a plethora of names; we've seen "verification #",
    // "verification number", "card identification number" and others listed
    // in the |pattern| below.
    if (is_ecml) {
      pattern = GetEcmlPattern(kEcmlCardVerification);
    } else {
      pattern = l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_CVC_RE);
    }

    if (credit_card_field->verification_ == NULL &&
        ParseText(&q, pattern, &credit_card_field->verification_))
      continue;

    // TODO(jhawkins): Parse the type select control.

    if (is_ecml)
      pattern = GetEcmlPattern(kEcmlCardNumber);
    else
      pattern = l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_NUMBER_RE);

    if (credit_card_field->number_ == NULL && ParseText(&q, pattern,
        &credit_card_field->number_))
      continue;

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
      pattern = l10n_util::GetStringUTF16(IDS_AUTOFILL_EXPIRATION_MONTH_RE);

    if ((!credit_card_field->expiration_month_ ||
        credit_card_field->expiration_month_->IsEmpty()) &&
        ParseText(&q, pattern, &credit_card_field->expiration_month_)) {
      if (is_ecml)
        pattern = GetEcmlPattern(kEcmlCardExpireYear);
      else
        pattern = l10n_util::GetStringUTF16(IDS_AUTOFILL_EXPIRATION_DATE_RE);

      if (!ParseText(&q, pattern, &credit_card_field->expiration_year_))
        return NULL;

      continue;
    }

    if (ParseText(&q, GetEcmlPattern(kEcmlCardExpireDay)))
      continue;

    // Some pages (e.g. ExpediaBilling.html) have a "card description"
    // field; we parse this field but ignore it.
    // We also ignore any other fields within a credit card block that
    // start with "card", under the assumption that they are related to
    // the credit card section being processed but are uninteresting to us.
    if (ParseText(&q, l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_IGNORED_RE)))
      continue;

    break;
  }

  // Some pages have a billing address field after the cardholder name field.
  // For that case, allow only just the cardholder name field.  The remaining
  // CC fields will be picked up in a following CreditCardField.
  if (credit_card_field->cardholder_) {
    *iter = q;
    return credit_card_field.release();
  }

  // On some pages, the user selects a card type using radio buttons
  // (e.g. test page Apple Store Billing.html).  We can't handle that yet,
  // so we treat the card type as optional for now.
  // The existence of a number or cvc in combination with expiration date is
  // a strong enough signal that this is a credit card.  It is possible that
  // the number and name were parsed in a separate part of the form.  So if
  // the cvc and date were found independently they are returned.
  if ((credit_card_field->number_ || credit_card_field->verification_) &&
      credit_card_field->expiration_month_ &&
      credit_card_field->expiration_year_) {
      *iter = q;
      return credit_card_field.release();
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
