// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_field.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "components/autofill/core/browser/autofill_scanner.h"
#include "components/autofill/core/browser/field_types.h"

using base::UTF8ToUTF16;

namespace autofill {

namespace {

bool SetFieldAndAdvanceCursor(AutofillScanner* scanner, AutofillField** field) {
  *field = scanner->Cursor();
  scanner->Advance();
  return true;
}

}  // namespace

// Some sites use type="tel" for zip fields (to get a numerical input).
// http://crbug.com/426958
const int AddressField::kZipCodeMatchType =
    MATCH_DEFAULT | MATCH_TELEPHONE | MATCH_NUMBER;

// Select fields are allowed here.  This occurs on top-100 site rediff.com.
const int AddressField::kCityMatchType = MATCH_DEFAULT | MATCH_SELECT;

const int AddressField::kStateMatchType = MATCH_DEFAULT | MATCH_SELECT;

scoped_ptr<FormField> AddressField::Parse(AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return NULL;

  scoped_ptr<AddressField> address_field(new AddressField);
  const AutofillField* const initial_field = scanner->Cursor();
  size_t saved_cursor = scanner->SaveCursor();

  base::string16 attention_ignored = UTF8ToUTF16(kAttentionIgnoredRe);
  base::string16 region_ignored = UTF8ToUTF16(kRegionIgnoredRe);

  // Allow address fields to appear in any order.
  size_t begin_trailing_non_labeled_fields = 0;
  bool has_trailing_non_labeled_fields = false;
  while (!scanner->IsEnd()) {
    const size_t cursor = scanner->SaveCursor();
    if (address_field->ParseAddressLines(scanner) ||
        address_field->ParseCityStateZipCode(scanner) ||
        address_field->ParseCountry(scanner) ||
        address_field->ParseCompany(scanner)) {
      has_trailing_non_labeled_fields = false;
      continue;
    } else if (ParseField(scanner, attention_ignored, NULL) ||
               ParseField(scanner, region_ignored, NULL)) {
      // We ignore the following:
      // * Attention.
      // * Province/Region/Other.
      continue;
    } else if (scanner->Cursor() != initial_field &&
               ParseEmptyLabel(scanner, NULL)) {
      // Ignore non-labeled fields within an address; the page
      // MapQuest Driving Directions North America.html contains such a field.
      // We only ignore such fields after we've parsed at least one other field;
      // otherwise we'd effectively parse address fields before other field
      // types after any non-labeled fields, and we want email address fields to
      // have precedence since some pages contain fields labeled
      // "Email address".
      if (!has_trailing_non_labeled_fields) {
        has_trailing_non_labeled_fields = true;
        begin_trailing_non_labeled_fields = cursor;
      }

      continue;
    } else {
      // No field found.
      break;
    }
  }

  // If we have identified any address fields in this field then it should be
  // added to the list of fields.
  if (address_field->company_ ||
      address_field->address1_ ||
      address_field->address2_ ||
      address_field->address3_ ||
      address_field->street_address_ ||
      address_field->city_ ||
      address_field->state_ ||
      address_field->zip_ ||
      address_field->zip4_ ||
      address_field->country_) {
    // Don't slurp non-labeled fields at the end into the address.
    if (has_trailing_non_labeled_fields)
      scanner->RewindTo(begin_trailing_non_labeled_fields);

    return address_field.Pass();
  }

  scanner->RewindTo(saved_cursor);
  return NULL;
}

AddressField::AddressField()
    : company_(NULL),
      address1_(NULL),
      address2_(NULL),
      address3_(NULL),
      street_address_(NULL),
      city_(NULL),
      state_(NULL),
      zip_(NULL),
      zip4_(NULL),
      country_(NULL) {
}

bool AddressField::ClassifyField(ServerFieldTypeMap* map) const {
  // The page can request the address lines as a single textarea input or as
  // multiple text fields (or not at all), but it shouldn't be possible to
  // request both.
  DCHECK(!(address1_ && street_address_));
  DCHECK(!(address2_ && street_address_));
  DCHECK(!(address3_ && street_address_));

  return AddClassification(company_, COMPANY_NAME, map) &&
         AddClassification(address1_, ADDRESS_HOME_LINE1, map) &&
         AddClassification(address2_, ADDRESS_HOME_LINE2, map) &&
         AddClassification(address3_, ADDRESS_HOME_LINE3, map) &&
         AddClassification(street_address_, ADDRESS_HOME_STREET_ADDRESS, map) &&
         AddClassification(city_, ADDRESS_HOME_CITY, map) &&
         AddClassification(state_, ADDRESS_HOME_STATE, map) &&
         AddClassification(zip_, ADDRESS_HOME_ZIP, map) &&
         AddClassification(country_, ADDRESS_HOME_COUNTRY, map);
}

bool AddressField::ParseCompany(AutofillScanner* scanner) {
  if (company_ && !company_->IsEmpty())
    return false;

  return ParseField(scanner, UTF8ToUTF16(kCompanyRe), &company_);
}

bool AddressField::ParseAddressLines(AutofillScanner* scanner) {
  // We only match the string "address" in page text, not in element names,
  // because sometimes every element in a group of address fields will have
  // a name containing the string "address"; for example, on the page
  // Kohl's - Register Billing Address.html the text element labeled "city"
  // has the name "BILL_TO_ADDRESS<>city".  We do match address labels
  // such as "address1", which appear as element names on various pages (eg
  // AmericanGirl-Registration.html, BloomingdalesBilling.html,
  // EBay Registration Enter Information.html).
  if (address1_ || street_address_)
    return false;

  // Ignore "Address Lookup" field. http://crbug.com/427622
  if (ParseField(scanner, base::UTF8ToUTF16(kAddressLookupRe), NULL))
    return false;

  base::string16 pattern = UTF8ToUTF16(kAddressLine1Re);
  base::string16 label_pattern = UTF8ToUTF16(kAddressLine1LabelRe);
  if (!ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT, &address1_) &&
      !ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                           &address1_) &&
      !ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT | MATCH_TEXT_AREA,
                           &street_address_) &&
      !ParseFieldSpecifics(scanner, label_pattern,
                           MATCH_LABEL | MATCH_TEXT_AREA,
                           &street_address_))
    return false;

  if (street_address_)
    return true;

  // This code may not pick up pages that have an address field consisting of a
  // sequence of unlabeled address fields. If we need to add this, see
  // discussion on https://codereview.chromium.org/741493003/
  pattern = UTF8ToUTF16(kAddressLine2Re);
  label_pattern = UTF8ToUTF16(kAddressLine2LabelRe);
  if (!ParseField(scanner, pattern, &address2_) &&
      !ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                           &address2_))
    return true;

  // Optionally parse address line 3. This uses the same label regexp as
  // address 2 above.
  pattern = UTF8ToUTF16(kAddressLinesExtraRe);
  if (!ParseField(scanner, pattern, &address3_) &&
      !ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                           &address3_))
    return true;

  // Try for surplus lines, which we will promptly discard. Some pages have 4
  // address lines (e.g. uk/ShoesDirect2.html)!
  //
  // Since these are rare, don't bother considering unlabeled lines as extra
  // address lines.
  pattern = UTF8ToUTF16(kAddressLinesExtraRe);
  while (ParseField(scanner, pattern, NULL)) {
    // Consumed a surplus line, try for another.
  }
  return true;
}

bool AddressField::ParseCountry(AutofillScanner* scanner) {
  if (country_ && !country_->IsEmpty())
    return false;

  scanner->SaveCursor();
  if (ParseFieldSpecifics(scanner,
                          UTF8ToUTF16(kCountryRe),
                          MATCH_DEFAULT | MATCH_SELECT,
                          &country_)) {
    return true;
  }

  // The occasional page (e.g. google account registration page) calls this a
  // "location". However, this only makes sense for select tags.
  scanner->Rewind();
  return ParseFieldSpecifics(scanner,
                             UTF8ToUTF16(kCountryLocationRe),
                             MATCH_LABEL | MATCH_NAME | MATCH_SELECT,
                             &country_);
}

bool AddressField::ParseZipCode(AutofillScanner* scanner) {
  if (zip_)
    return false;

  if (!ParseFieldSpecifics(scanner,
                           UTF8ToUTF16(kZipCodeRe),
                           kZipCodeMatchType,
                           &zip_)) {
    return false;
  }

  // Look for a zip+4, whose field name will also often contain
  // the substring "zip".
  ParseFieldSpecifics(scanner, UTF8ToUTF16(kZip4Re), kZipCodeMatchType, &zip4_);
  return true;
}

bool AddressField::ParseCity(AutofillScanner* scanner) {
  if (city_)
    return false;

  return ParseFieldSpecifics(scanner,
                             UTF8ToUTF16(kCityRe),
                             kCityMatchType,
                             &city_);
}

bool AddressField::ParseState(AutofillScanner* scanner) {
  if (state_)
    return false;

  return ParseFieldSpecifics(scanner,
                             UTF8ToUTF16(kStateRe),
                             kStateMatchType,
                             &state_);
}

bool AddressField::ParseCityStateZipCode(AutofillScanner* scanner) {
  // Simple cases.
  if (scanner->IsEnd())
    return false;
  if (city_ && state_ && zip_)
    return false;
  if (state_ && zip_)
    return ParseCity(scanner);
  if (city_ && zip_)
    return ParseState(scanner);
  if (city_ && state_)
    return ParseZipCode(scanner);

  // Check for matches to both name and label.
  ParseNameLabelResult city_result = ParseNameAndLabelForCity(scanner);
  if (city_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult state_result = ParseNameAndLabelForState(scanner);
  if (state_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult zip_result = ParseNameAndLabelForZipCode(scanner);
  if (zip_result == RESULT_MATCH_NAME_LABEL)
    return true;

  // Check if there is only one potential match.
  bool maybe_city = city_result != RESULT_MATCH_NONE;
  bool maybe_state = state_result != RESULT_MATCH_NONE;
  bool maybe_zip = zip_result != RESULT_MATCH_NONE;
  if (maybe_city && !maybe_state && !maybe_zip)
    return SetFieldAndAdvanceCursor(scanner, &city_);
  if (maybe_state && !maybe_city && !maybe_zip)
    return SetFieldAndAdvanceCursor(scanner, &state_);
  if (maybe_zip && !maybe_city && !maybe_state)
    return ParseZipCode(scanner);

  // Otherwise give name priority over label.
  if (city_result == RESULT_MATCH_NAME)
    return SetFieldAndAdvanceCursor(scanner, &city_);
  if (state_result == RESULT_MATCH_NAME)
    return SetFieldAndAdvanceCursor(scanner, &state_);
  if (zip_result == RESULT_MATCH_NAME)
    return ParseZipCode(scanner);

  if (city_result == RESULT_MATCH_LABEL)
    return SetFieldAndAdvanceCursor(scanner, &city_);
  if (state_result == RESULT_MATCH_LABEL)
    return SetFieldAndAdvanceCursor(scanner, &state_);
  if (zip_result == RESULT_MATCH_LABEL)
    return ParseZipCode(scanner);

  return false;
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForZipCode(
    AutofillScanner* scanner) {
  if (zip_)
    return RESULT_MATCH_NONE;

  ParseNameLabelResult result = ParseNameAndLabelSeparately(
      scanner, UTF8ToUTF16(kZipCodeRe), kZipCodeMatchType, &zip_);

  if (result != RESULT_MATCH_NAME_LABEL || scanner->IsEnd())
    return result;

  size_t saved_cursor = scanner->SaveCursor();
  bool found_non_zip4 = ParseCity(scanner);
  if (found_non_zip4)
    city_ = nullptr;
  scanner->RewindTo(saved_cursor);
  if (!found_non_zip4) {
    found_non_zip4 = ParseState(scanner);
    if (found_non_zip4)
      state_ = nullptr;
    scanner->RewindTo(saved_cursor);
  }

  if (!found_non_zip4) {
    // Look for a zip+4, whose field name will also often contain
    // the substring "zip".
    ParseFieldSpecifics(scanner,
                        UTF8ToUTF16(kZip4Re),
                        kZipCodeMatchType,
                        &zip4_);
  }
  return result;
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForCity(
    AutofillScanner* scanner) {
  if (city_)
    return RESULT_MATCH_NONE;

  return ParseNameAndLabelSeparately(
      scanner, UTF8ToUTF16(kCityRe), kCityMatchType, &city_);
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForState(
    AutofillScanner* scanner) {
  if (state_)
    return RESULT_MATCH_NONE;

  return ParseNameAndLabelSeparately(
      scanner, UTF8ToUTF16(kStateRe), kStateMatchType, &state_);
}

}  // namespace autofill
