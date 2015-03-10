// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/credit_card_field.h"

#include <stddef.h>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "components/autofill/core/browser/autofill_scanner.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// Credit card numbers are at most 19 digits in length.
// [Ref: http://en.wikipedia.org/wiki/Bank_card_number]
static const size_t kMaxValidCardNumberSize = 19;

// static
scoped_ptr<FormField> CreditCardField::Parse(AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return NULL;

  scoped_ptr<CreditCardField> credit_card_field(new CreditCardField);
  size_t saved_cursor = scanner->SaveCursor();

  // Credit card fields can appear in many different orders.
  // We loop until no more credit card related fields are found, see |break| at
  // bottom of the loop.
  for (int fields = 0; !scanner->IsEnd(); ++fields) {
    // Ignore gift card fields.
    if (ParseField(scanner, base::UTF8ToUTF16(kGiftCardRe), NULL))
      break;

    if (!credit_card_field->cardholder_) {
      if (ParseField(scanner,
                     base::UTF8ToUTF16(kNameOnCardRe),
                     &credit_card_field->cardholder_)) {
        continue;
      }

      // Sometimes the cardholder field is just labeled "name". Unfortunately
      // this is a dangerously generic word to search for, since it will often
      // match a name (not cardholder name) field before or after credit card
      // fields. So we search for "name" only when we've already parsed at
      // least one other credit card field and haven't yet parsed the
      // expiration date (which usually appears at the end).
      if (fields > 0 &&
          !credit_card_field->expiration_month_ &&
          ParseField(scanner,
                     base::UTF8ToUTF16(kNameOnCardContextualRe),
                     &credit_card_field->cardholder_)) {
        continue;
      }
    }

    // Check for a credit card type (Visa, MasterCard, etc.) field.
    if (!credit_card_field->type_ &&
        ParseFieldSpecifics(scanner,
                            base::UTF8ToUTF16(kCardTypeRe),
                            MATCH_DEFAULT | MATCH_SELECT,
                            &credit_card_field->type_)) {
      continue;
    }

    // We look for a card security code before we look for a credit card number
    // and match the general term "number". The security code has a plethora of
    // names; we've seen "verification #", "verification number", "card
    // identification number", and others listed in the regex pattern used
    // below.
    // Note: Some sites use type="tel" or type="number" for numerical inputs.
    const int kMatchNumAndTel = MATCH_DEFAULT | MATCH_NUMBER | MATCH_TELEPHONE;
    if (!credit_card_field->verification_ &&
        ParseFieldSpecifics(scanner,
                            base::UTF8ToUTF16(kCardCvcRe),
                            kMatchNumAndTel | MATCH_PASSWORD,
                            &credit_card_field->verification_)) {
      continue;
    }

    AutofillField* current_number_field;
    if (ParseFieldSpecifics(scanner,
                            base::UTF8ToUTF16(kCardNumberRe),
                            kMatchNumAndTel,
                            &current_number_field)) {
      // Avoid autofilling any credit card number field having very low or high
      // |start_index| on the HTML form.
      size_t start_index = 0;
      if (!credit_card_field->numbers_.empty()) {
        size_t last_number_field_size =
            credit_card_field->numbers_.back()->credit_card_number_offset() +
            credit_card_field->numbers_.back()->max_length;

        // Distinguish between
        //   (a) one card split across multiple fields
        //   (b) multiple fields for multiple cards
        // Treat this field as a part of the same card as the last field, except
        // when doing so would cause overflow.
        if (last_number_field_size < kMaxValidCardNumberSize)
          start_index = last_number_field_size;
      }

      current_number_field->set_credit_card_number_offset(start_index);
      credit_card_field->numbers_.push_back(current_number_field);
      continue;
    }

    if (!credit_card_field->expiration_date_ &&
        LowerCaseEqualsASCII(scanner->Cursor()->form_control_type, "month")) {
      credit_card_field->expiration_date_ = scanner->Cursor();
      credit_card_field->expiration_month_ = nullptr;
      credit_card_field->expiration_year_ = nullptr;
      scanner->Advance();
      continue;
    }

    if (!credit_card_field->expiration_month_ &&
        !credit_card_field->expiration_date_) {
      // First try to parse split month/year expiration fields.
      scanner->SaveCursor();
      if (ParseFieldSpecifics(scanner,
                              base::UTF8ToUTF16(kExpirationMonthRe),
                              MATCH_DEFAULT | MATCH_SELECT,
                              &credit_card_field->expiration_month_) &&
          ParseFieldSpecifics(scanner,
                              base::UTF8ToUTF16(kExpirationYearRe),
                              MATCH_DEFAULT | MATCH_SELECT,
                              &credit_card_field->expiration_year_)) {
        continue;
      }

      // If that fails, try to parse a combined expiration field.
      // Look for a 2-digit year first.
      // We allow <select> fields, because they're used e.g. on qvc.com.
      scanner->Rewind();
      if (ParseFieldSpecifics(scanner,
                              base::UTF8ToUTF16(kExpirationDate2DigitYearRe),
                              MATCH_LABEL | MATCH_TEXT | MATCH_SELECT,
                              &credit_card_field->expiration_date_)) {
        credit_card_field->exp_year_type_ = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
        credit_card_field->expiration_month_ = nullptr;
        continue;
      }

      if (ParseFieldSpecifics(scanner,
                              base::UTF8ToUTF16(kExpirationDateRe),
                              MATCH_LABEL | MATCH_TEXT | MATCH_SELECT,
                              &credit_card_field->expiration_date_)) {
        credit_card_field->expiration_month_ = nullptr;
        continue;
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
    if (ParseField(scanner, base::UTF8ToUTF16(kCardIgnoredRe), NULL))
      continue;

    break;
  }

  // Some pages have a billing address field after the cardholder name field.
  // For that case, allow only just the cardholder name field.  The remaining
  // CC fields will be picked up in a following CreditCardField.
  if (credit_card_field->cardholder_)
    return credit_card_field.Pass();

  // On some pages, the user selects a card type using radio buttons
  // (e.g. test page Apple Store Billing.html).  We can't handle that yet,
  // so we treat the card type as optional for now.
  // The existence of a number or cvc in combination with expiration date is
  // a strong enough signal that this is a credit card.  It is possible that
  // the number and name were parsed in a separate part of the form.  So if
  // the cvc and date were found independently they are returned.
  bool has_cc_number_or_verification = (credit_card_field->verification_ ||
                                        !credit_card_field->numbers_.empty());
  bool has_date_or_mm_yy = (credit_card_field->expiration_date_ ||
                            (credit_card_field->expiration_month_ &&
                             credit_card_field->expiration_year_));
  if (has_cc_number_or_verification && has_date_or_mm_yy)
    return credit_card_field.Pass();

  scanner->RewindTo(saved_cursor);
  return NULL;
}

CreditCardField::CreditCardField()
    : cardholder_(NULL),
      cardholder_last_(NULL),
      type_(NULL),
      verification_(NULL),
      expiration_month_(NULL),
      expiration_year_(NULL),
      expiration_date_(NULL),
      exp_year_type_(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) {
}

CreditCardField::~CreditCardField() {
}

bool CreditCardField::ClassifyField(ServerFieldTypeMap* map) const {
  bool ok = true;
  for (size_t index = 0; index < numbers_.size(); ++index) {
    ok = ok && AddClassification(numbers_[index], CREDIT_CARD_NUMBER, map);
  }

  ok = ok && AddClassification(type_, CREDIT_CARD_TYPE, map);
  ok = ok &&
       AddClassification(verification_, CREDIT_CARD_VERIFICATION_CODE, map);

  // If the heuristics detected first and last name in separate fields,
  // then ignore both fields. Putting them into separate fields is probably
  // wrong, because the credit card can also contain a middle name or middle
  // initial.
  if (cardholder_last_ == NULL)
    ok = ok && AddClassification(cardholder_, CREDIT_CARD_NAME, map);

  if (expiration_date_) {
    DCHECK(!expiration_month_);
    DCHECK(!expiration_year_);
    ok =
        ok && AddClassification(expiration_date_, GetExpirationYearType(), map);
  } else {
    ok = ok && AddClassification(expiration_month_, CREDIT_CARD_EXP_MONTH, map);
    ok =
        ok && AddClassification(expiration_year_, GetExpirationYearType(), map);
  }

  return ok;
}

ServerFieldType CreditCardField::GetExpirationYearType() const {
  return (expiration_date_
              ? exp_year_type_
              : ((expiration_year_ && expiration_year_->max_length == 2)
                     ? CREDIT_CARD_EXP_2_DIGIT_YEAR
                     : CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

}  // namespace autofill
