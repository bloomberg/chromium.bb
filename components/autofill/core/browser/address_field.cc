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
#include "ui/base/l10n/l10n_util.h"

using base::UTF8ToUTF16;

namespace autofill {

FormField* AddressField::Parse(AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return NULL;

  scoped_ptr<AddressField> address_field(new AddressField);
  const AutofillField* const initial_field = scanner->Cursor();
  size_t saved_cursor = scanner->SaveCursor();

  base::string16 attention_ignored = UTF8ToUTF16(autofill::kAttentionIgnoredRe);
  base::string16 region_ignored = UTF8ToUTF16(autofill::kRegionIgnoredRe);

  // Allow address fields to appear in any order.
  size_t begin_trailing_non_labeled_fields = 0;
  bool has_trailing_non_labeled_fields = false;
  while (!scanner->IsEnd()) {
    const size_t cursor = scanner->SaveCursor();
    if (address_field->ParseAddressLines(scanner) ||
        address_field->ParseCity(scanner) ||
        address_field->ParseState(scanner) ||
        address_field->ParseZipCode(scanner) ||
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
      address_field->street_address_ ||
      address_field->city_ ||
      address_field->state_ ||
      address_field->zip_ ||
      address_field->zip4_ ||
      address_field->country_) {
    // Don't slurp non-labeled fields at the end into the address.
    if (has_trailing_non_labeled_fields)
      scanner->RewindTo(begin_trailing_non_labeled_fields);

    return address_field.release();
  }

  scanner->RewindTo(saved_cursor);
  return NULL;
}

AddressField::AddressField()
    : company_(NULL),
      address1_(NULL),
      address2_(NULL),
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

  return AddClassification(company_, COMPANY_NAME, map) &&
         AddClassification(address1_, ADDRESS_HOME_LINE1, map) &&
         AddClassification(address2_, ADDRESS_HOME_LINE2, map) &&
         AddClassification(street_address_, ADDRESS_HOME_STREET_ADDRESS, map) &&
         AddClassification(city_, ADDRESS_HOME_CITY, map) &&
         AddClassification(state_, ADDRESS_HOME_STATE, map) &&
         AddClassification(zip_, ADDRESS_HOME_ZIP, map) &&
         AddClassification(country_, ADDRESS_HOME_COUNTRY, map);
}

bool AddressField::ParseCompany(AutofillScanner* scanner) {
  if (company_ && !company_->IsEmpty())
    return false;

  return ParseField(scanner, UTF8ToUTF16(autofill::kCompanyRe), &company_);
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

  base::string16 pattern = UTF8ToUTF16(autofill::kAddressLine1Re);
  base::string16 label_pattern = UTF8ToUTF16(autofill::kAddressLine1LabelRe);
  if (!ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT, &address1_) &&
      !ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                           &address1_) &&
      !ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT | MATCH_TEXT_AREA,
                           &street_address_) &&
      !ParseFieldSpecifics(scanner, label_pattern,
                           MATCH_LABEL | MATCH_TEXT_AREA,
                           &street_address_)) {
    return false;
  }

  // Optionally parse more address lines, which may have empty labels.
  pattern = UTF8ToUTF16(autofill::kAddressLine2Re);
  label_pattern = UTF8ToUTF16(autofill::kAddressLine2LabelRe);
  if (!street_address_ &&
      !ParseEmptyLabel(scanner, &address2_) &&
      !ParseField(scanner, pattern, &address2_)) {
    ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                        &address2_);
  }

  // Try for surplus lines, which we will promptly discard.
  // Some pages have 3 address lines (eg SharperImageModifyAccount.html)
  // Some pages even have 4 address lines (e.g. uk/ShoesDirect2.html)!
  if (address2_) {
    pattern = UTF8ToUTF16(autofill::kAddressLinesExtraRe);
    while (ParseField(scanner, pattern, NULL)) {
      // Consumed a surplus line, try for another.
    }
  }

  return true;
}

bool AddressField::ParseCountry(AutofillScanner* scanner) {
  // Parse a country.  The occasional page (e.g.
  // Travelocity_New Member Information1.html) calls this a "location".
  if (country_ && !country_->IsEmpty())
    return false;

  return ParseFieldSpecifics(scanner,
                             UTF8ToUTF16(autofill::kCountryRe),
                             MATCH_DEFAULT | MATCH_SELECT,
                             &country_);
}

bool AddressField::ParseZipCode(AutofillScanner* scanner) {
  // Parse a zip code.  On some UK pages (e.g. The China Shop2.html) this
  // is called a "post code".
  if (zip_)
    return false;

  base::string16 pattern = UTF8ToUTF16(autofill::kZipCodeRe);
  if (!ParseField(scanner, pattern, &zip_))
    return false;

  // Look for a zip+4, whose field name will also often contain
  // the substring "zip".
  ParseField(scanner, UTF8ToUTF16(autofill::kZip4Re), &zip4_);
  return true;
}

bool AddressField::ParseCity(AutofillScanner* scanner) {
  // Parse a city name.  Some UK pages (e.g. The China Shop2.html) use
  // the term "town".
  if (city_)
    return false;

  // Select fields are allowed here.  This occurs on top-100 site rediff.com.
  return ParseFieldSpecifics(scanner,
                             UTF8ToUTF16(autofill::kCityRe),
                             MATCH_DEFAULT | MATCH_SELECT,
                             &city_);
}

bool AddressField::ParseState(AutofillScanner* scanner) {
  if (state_)
    return false;

  return ParseFieldSpecifics(scanner,
                             UTF8ToUTF16(autofill::kStateRe),
                             MATCH_DEFAULT | MATCH_SELECT,
                             &state_);
}

}  // namespace autofill
