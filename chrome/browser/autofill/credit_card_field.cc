// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card_field.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/autofill_regex_constants.h"
#include "chrome/browser/autofill/autofill_scanner.h"
#include "chrome/browser/autofill/field_types.h"
#include "ui/base/l10n/l10n_util.h"

// static
FormField* CreditCardField::Parse(AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return NULL;

  scoped_ptr<CreditCardField> credit_card_field(new CreditCardField);
  size_t saved_cursor = scanner->SaveCursor();

  // Credit card fields can appear in many different orders.
  // We loop until no more credit card related fields are found, see |break| at
  // bottom of the loop.
  for (int fields = 0; !scanner->IsEnd(); ++fields) {
    // Ignore gift card fields.
    if (ParseField(scanner, UTF8ToUTF16(autofill::kGiftCardRe), NULL))
      break;

    // Sometimes the cardholder field is just labeled "name". Unfortunately this
    // is a dangerously generic word to search for, since it will often match a
    // name (not cardholder name) field before or after credit card fields. So
    // we search for "name" only when we've already parsed at least one other
    // credit card field and haven't yet parsed the expiration date (which
    // usually appears at the end).
    if (credit_card_field->cardholder_ == NULL) {
      string16 name_pattern;
      if (fields == 0 || credit_card_field->expiration_month_) {
        // at beginning or end
        name_pattern = UTF8ToUTF16(autofill::kNameOnCardRe);
      } else {
        name_pattern = UTF8ToUTF16(autofill::kNameOnCardContextualRe);
      }

      if (ParseField(scanner, name_pattern, &credit_card_field->cardholder_))
        continue;

      // As a hard-coded hack for Expedia's billing pages (expedia_checkout.html
      // and ExpediaBilling.html in our test suite), recognize separate fields
      // for the cardholder's first and last name if they have the labels "cfnm"
      // and "clnm".
      scanner->SaveCursor();
      const AutofillField* first;
      if (ParseField(scanner, ASCIIToUTF16("^cfnm"), &first) &&
          ParseField(scanner, ASCIIToUTF16("^clnm"),
                     &credit_card_field->cardholder_last_)) {
        credit_card_field->cardholder_ = first;
        continue;
      }
      scanner->Rewind();
    }

    // Check for a credit card type (Visa, MasterCard, etc.) field.
    string16 type_pattern = UTF8ToUTF16(autofill::kCardTypeRe);
    if (!credit_card_field->type_ &&
        ParseFieldSpecifics(scanner, type_pattern,
                            MATCH_DEFAULT | MATCH_SELECT,
                            &credit_card_field->type_)) {
      continue;
    }

    // We look for a card security code before we look for a credit
    // card number and match the general term "number".  The security code
    // has a plethora of names; we've seen "verification #",
    // "verification number", "card identification number" and others listed
    // in the |pattern| below.
    string16 pattern = UTF8ToUTF16(autofill::kCardCvcRe);
    if (!credit_card_field->verification_ &&
        ParseField(scanner, pattern, &credit_card_field->verification_)) {
      continue;
    }

    pattern = UTF8ToUTF16(autofill::kCardNumberRe);
    if (!credit_card_field->number_ &&
        ParseField(scanner, pattern, &credit_card_field->number_)) {
      continue;
    }

    if (LowerCaseEqualsASCII(scanner->Cursor()->form_control_type, "month")) {
      credit_card_field->expiration_month_ = scanner->Cursor();
      scanner->Advance();
    } else {
      // First try to parse split month/year expiration fields.
      scanner->SaveCursor();
      pattern = UTF8ToUTF16(autofill::kExpirationMonthRe);
      if (!credit_card_field->expiration_month_ &&
          ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT | MATCH_SELECT,
                              &credit_card_field->expiration_month_)) {
        pattern = UTF8ToUTF16(autofill::kExpirationYearRe);
        if (ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT | MATCH_SELECT,
                                 &credit_card_field->expiration_year_)) {
          continue;
        }
      }

      // If that fails, try to parse a combined expiration field.
      if (!credit_card_field->expiration_date_) {
        // Look for a 2-digit year first.
        scanner->Rewind();
        pattern = UTF8ToUTF16(autofill::kExpirationDate2DigitYearRe);
        // We allow <select> fields, because they're used e.g. on qvc.com.
        if (ParseFieldSpecifics(scanner, pattern,
                                MATCH_LABEL | MATCH_VALUE | MATCH_TEXT |
                                    MATCH_SELECT,
                                &credit_card_field->expiration_date_)) {
          credit_card_field->is_two_digit_year_ = true;
          continue;
        }

        pattern = UTF8ToUTF16(autofill::kExpirationDateRe);
        if (ParseFieldSpecifics(scanner, pattern,
                                MATCH_LABEL | MATCH_VALUE | MATCH_TEXT |
                                    MATCH_SELECT,
                                &credit_card_field->expiration_date_)) {
          continue;
        }
      }

      if (credit_card_field->expiration_month_ &&
          !credit_card_field->expiration_year_ &&
          !credit_card_field->expiration_date_) {
        // Parsed a month but couldn't parse a year; give up.
        scanner->RewindTo(saved_cursor);
        return NULL;
      }
    }

    // Some pages (e.g. ExpediaBilling.html) have a "card description"
    // field; we parse this field but ignore it.
    // We also ignore any other fields within a credit card block that
    // start with "card", under the assumption that they are related to
    // the credit card section being processed but are uninteresting to us.
    if (ParseField(scanner, UTF8ToUTF16(autofill::kCardIgnoredRe), NULL))
      continue;

    break;
  }

  // Some pages have a billing address field after the cardholder name field.
  // For that case, allow only just the cardholder name field.  The remaining
  // CC fields will be picked up in a following CreditCardField.
  if (credit_card_field->cardholder_)
    return credit_card_field.release();

  // On some pages, the user selects a card type using radio buttons
  // (e.g. test page Apple Store Billing.html).  We can't handle that yet,
  // so we treat the card type as optional for now.
  // The existence of a number or cvc in combination with expiration date is
  // a strong enough signal that this is a credit card.  It is possible that
  // the number and name were parsed in a separate part of the form.  So if
  // the cvc and date were found independently they are returned.
  if ((credit_card_field->number_ || credit_card_field->verification_) &&
      (credit_card_field->expiration_date_ ||
       (credit_card_field->expiration_month_ &&
        (credit_card_field->expiration_year_ ||
         (LowerCaseEqualsASCII(
             credit_card_field->expiration_month_->form_control_type,
             "month")))))) {
    return credit_card_field.release();
  }

  scanner->RewindTo(saved_cursor);
  return NULL;
}

CreditCardField::CreditCardField()
    : cardholder_(NULL),
      cardholder_last_(NULL),
      type_(NULL),
      number_(NULL),
      verification_(NULL),
      expiration_month_(NULL),
      expiration_year_(NULL),
      expiration_date_(NULL),
      is_two_digit_year_(false) {
}

bool CreditCardField::ClassifyField(FieldTypeMap* map) const {
  bool ok = AddClassification(number_, CREDIT_CARD_NUMBER, map);
  ok = ok && AddClassification(type_, CREDIT_CARD_TYPE, map);
  ok = ok && AddClassification(verification_, CREDIT_CARD_VERIFICATION_CODE,
                               map);

  // If the heuristics detected first and last name in separate fields,
  // then ignore both fields. Putting them into separate fields is probably
  // wrong, because the credit card can also contain a middle name or middle
  // initial.
  if (cardholder_last_ == NULL)
    ok = ok && AddClassification(cardholder_, CREDIT_CARD_NAME, map);

  if (expiration_date_) {
    if (is_two_digit_year_) {
      ok = ok && AddClassification(expiration_date_,
                                   CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, map);
    } else {
      ok = ok && AddClassification(expiration_date_,
                                   CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, map);
    }
  } else {
    ok = ok && AddClassification(expiration_month_, CREDIT_CARD_EXP_MONTH, map);
    if (is_two_digit_year_) {
      ok = ok && AddClassification(expiration_year_,
                                   CREDIT_CARD_EXP_2_DIGIT_YEAR,
                                   map);
    } else {
      ok = ok && AddClassification(expiration_year_,
                                   CREDIT_CARD_EXP_4_DIGIT_YEAR,
                                   map);
    }
  }

  return ok;
}
