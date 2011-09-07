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
#include "chrome/browser/autofill/autofill_scanner.h"
#include "chrome/browser/autofill/field_types.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kNameOnCardRe[] =
    "card.?holder|name.?on.?card|ccname|ccfullname|owner"
    // de-DE: |karteninhaber
    "|karteninhaber"
    // es: |nombre.*tarjeta
    "|nombre.*tarjeta"
    // fr-FR: |nom.*carte
    "|nom.*carte"
    // it-IT: |nome.*cart
    "|nome.*cart"
    // ja-JP: |名前
    "|\xe5\x90\x8d\xe5\x89\x8d"
    // ru: |Имя.*карты
    "|\xd0\x98\xd0\xbc\xd1\x8f.*\xd0\xba\xd0\xb0\xd1\x80\xd1\x82\xd1\x8b"
    // zh-CN: |信用卡开户名|开户名|持卡人姓名
    "|\xe4\xbf\xa1\xe7\x94\xa8\xe5\x8d\xa1\xe5\xbc\x80\xe6\x88\xb7\xe5\x90\x8d"
        "|\xe5\xbc\x80\xe6\x88\xb7\xe5\x90\x8d|\xe6\x8c\x81\xe5\x8d\xa1\xe4"
        "\xba\xba\xe5\xa7\x93\xe5\x90\x8d"
    // zh-TW: |持卡人姓名
    "|\xe6\x8c\x81\xe5\x8d\xa1\xe4\xba\xba\xe5\xa7\x93\xe5\x90\x8d";
const char kNameOnCardContextualRe[] =
    "name";
const char kCardNumberRe[] =
    "card.?number|card.?#|card.?no|ccnum|acctnum"
    // de-DE: |nummer
    "|nummer"
    // es: |credito|numero|número
    "|credito|numero|n\xc3\xbamero"
    // fr-FR: |numéro
    "|num\xc3\xa9ro"
    // ja-JP: |カード番号
    "|\xe3\x82\xab\xe3\x83\xbc\xe3\x83\x89\xe7\x95\xaa\xe5\x8f\xb7"
    // ru: |Номер.*карты
    "|\xd0\x9d\xd0\xbe\xd0\xbc\xd0\xb5\xd1\x80.*\xd0\xba\xd0\xb0\xd1\x80\xd1"
        "\x82\xd1\x8b"
    // zh-CN: |信用卡号|信用卡号码
    "|\xe4\xbf\xa1\xe7\x94\xa8\xe5\x8d\xa1\xe5\x8f\xb7|\xe4\xbf\xa1\xe7\x94"
        "\xa8\xe5\x8d\xa1\xe5\x8f\xb7\xe7\xa0\x81"
    // zh-TW: |信用卡卡號
    "|\xe4\xbf\xa1\xe7\x94\xa8\xe5\x8d\xa1\xe5\x8d\xa1\xe8\x99\x9f"
    // ko-KR: |카드
    "|\xec\xb9\xb4\xeb\x93\x9c";
const char kCardCvcRe[] =
    "verification|card identification|security code|cvn|cvv|cvc|csc";

// "Expiration date" is the most common label here, but some pages have
// "Expires", "exp. date" or "exp. month" and "exp. year".  We also look
// for the field names ccmonth and ccyear, which appear on at least 4 of
// our test pages.

// On at least one page (The China Shop2.html) we find only the labels
// "month" and "year".  So for now we match these words directly; we'll
// see if this turns out to be too general.

// Toolbar Bug 51451: indeed, simply matching "month" is too general for
//   https://rps.fidelity.com/ftgw/rps/RtlCust/CreatePIN/Init.
// Instead, we match only words beginning with "month".
const char kExpirationMonthRe[] =
    "expir|exp.*mo|exp.*date|ccmonth"
    // de-DE: |gueltig|gültig|monat
    "|gueltig|g\xc3\xbcltig|monat"
    // es: |fecha
    "|fecha"
    // fr-FR: |date.*exp
    "|date.*exp"
    // it-IT: |scadenza
    "|scadenza"
    // ja-JP: |有効期限
    "|\xe6\x9c\x89\xe5\x8a\xb9\xe6\x9c\x9f\xe9\x99\x90"
    // pt-BR, pt-PT: |validade
    "|validade"
    // ru: |Срок действия карты
    "|\xd0\xa1\xd1\x80\xd0\xbe\xd0\xba \xd0\xb4\xd0\xb5\xd0\xb9\xd1\x81\xd1"
        "\x82\xd0\xb2\xd0\xb8\xd1\x8f \xd0\xba\xd0\xb0\xd1\x80\xd1\x82\xd1\x8b"
    // zh-CN: |月
    "|\xe6\x9c\x88";
const char kExpirationYearRe[] =
    "exp|^/|year"
    // de-DE: |ablaufdatum|gueltig|gültig|yahr
    "|ablaufdatum|gueltig|g\xc3\xbcltig|yahr"
    // es: |fecha
    "|fecha"
    // it-IT: |scadenza
    "|scadenza"
    // ja-JP: |有効期限
    "|\xe6\x9c\x89\xe5\x8a\xb9\xe6\x9c\x9f\xe9\x99\x90"
    // pt-BR, pt-PT: |validade
    "|validade"
    // ru: |Срок действия карты
    "|\xd0\xa1\xd1\x80\xd0\xbe\xd0\xba \xd0\xb4\xd0\xb5\xd0\xb9\xd1\x81\xd1"
        "\x82\xd0\xb2\xd0\xb8\xd1\x8f \xd0\xba\xd0\xb0\xd1\x80\xd1\x82\xd1\x8b"
    // zh-CN: |年|有效期
    "|\xe5\xb9\xb4|\xe6\x9c\x89\xe6\x95\x88\xe6\x9c\x9f";

// This regex is a little bit nasty, but it is simply requiring exactly two
// adjacent y's.
const char kExpirationDate2DigitYearRe[] =
    "exp.*date.*[^y]yy([^y]|$)";
const char kExpirationDateRe[] =
    "expir|exp.*date"
    // de-DE: |gueltig|gültig
    "|gueltig|g\xc3\xbcltig"
    // es: |fecha
    "|fecha"
    // fr-FR: |date.*exp
    "|date.*exp"
    // it-IT: |scadenza
    "|scadenza"
    // ja-JP: |有効期限
    "|\xe6\x9c\x89\xe5\x8a\xb9\xe6\x9c\x9f\xe9\x99\x90"
    // pt-BR, pt-PT: |validade
    "|validade"
    // ru: |Срок действия карты
    "|\xd0\xa1\xd1\x80\xd0\xbe\xd0\xba \xd0\xb4\xd0\xb5\xd0\xb9\xd1\x81\xd1"
        "\x82\xd0\xb2\xd0\xb8\xd1\x8f\xd0\xba\xd0\xb0\xd1\x80\xd1\x82\xd1\x8b";
const char kCardIgnoredRe[] =
    "^card";

}  // namespace

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
        name_pattern = UTF8ToUTF16(kNameOnCardRe);
      } else {
        name_pattern = UTF8ToUTF16(kNameOnCardContextualRe);
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

    // We look for a card security code before we look for a credit
    // card number and match the general term "number".  The security code
    // has a plethora of names; we've seen "verification #",
    // "verification number", "card identification number" and others listed
    // in the |pattern| below.
    string16 pattern = UTF8ToUTF16(kCardCvcRe);
    if (!credit_card_field->verification_ &&
        ParseField(scanner, pattern, &credit_card_field->verification_)) {
      continue;
    }
    // TODO(jhawkins): Parse the type select control.

    pattern = UTF8ToUTF16(kCardNumberRe);
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
      pattern = UTF8ToUTF16(kExpirationMonthRe);
      if (!credit_card_field->expiration_month_ &&
          ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT | MATCH_SELECT,
                              &credit_card_field->expiration_month_)) {
        pattern = UTF8ToUTF16(kExpirationYearRe);
        if (ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT | MATCH_SELECT,
                                 &credit_card_field->expiration_year_)) {
          continue;
        }
      }

      // If that fails, try to parse a combined expiration field.
      if (!credit_card_field->expiration_date_) {
        // Look for a 2-digit year first.
        scanner->Rewind();
        pattern = UTF8ToUTF16(kExpirationDate2DigitYearRe);
        if (ParseFieldSpecifics(scanner, pattern,
                                MATCH_LABEL | MATCH_VALUE | MATCH_TEXT,
                                &credit_card_field->expiration_date_)) {
          credit_card_field->is_two_digit_year_ = true;
          continue;
        }

        pattern = UTF8ToUTF16(kExpirationDateRe);
        if (ParseFieldSpecifics(scanner, pattern,
                                MATCH_LABEL | MATCH_VALUE | MATCH_TEXT,
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
    if (ParseField(scanner, UTF8ToUTF16(kCardIgnoredRe), NULL)) {
      continue;
    }

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

  // If the heuristics detected first and last name in separate fields,
  // then ignore both fields. Putting them into separate fields is probably
  // wrong, because the credit card can also contain a middle name or middle
  // initial.
  if (cardholder_last_ == NULL)
    ok = ok && AddClassification(cardholder_, CREDIT_CARD_NAME, map);

  ok = ok && AddClassification(type_, CREDIT_CARD_TYPE, map);
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
