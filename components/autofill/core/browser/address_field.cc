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
  if (address_field->company_ != NULL ||
      address_field->address1_ != NULL || address_field->address2_ != NULL ||
      address_field->city_ != NULL || address_field->state_ != NULL ||
      address_field->zip_ != NULL || address_field->zip4_ ||
      address_field->country_ != NULL) {
    // Don't slurp non-labeled fields at the end into the address.
    if (has_trailing_non_labeled_fields)
      scanner->RewindTo(begin_trailing_non_labeled_fields);

    address_field->type_ = address_field->FindType();
    return address_field.release();
  }

  scanner->RewindTo(saved_cursor);
  return NULL;
}

AddressField::AddressType AddressField::FindType() const {
  // First look at the field name, which itself will sometimes contain
  // "bill" or "ship".
  if (company_)
    return AddressTypeFromText(company_->name);

  if (address1_)
    return AddressTypeFromText(address1_->name);

  if (address2_)
    return AddressTypeFromText(address2_->name);

  if (city_)
    return AddressTypeFromText(city_->name);

  if (zip_)
    return AddressTypeFromText(zip_->name);

  if (state_)
    return AddressTypeFromText(state_->name);

  if (country_)
    return AddressTypeFromText(country_->name);

  return kGenericAddress;
}

AddressField::AddressField()
    : company_(NULL),
      address1_(NULL),
      address2_(NULL),
      city_(NULL),
      state_(NULL),
      zip_(NULL),
      zip4_(NULL),
      country_(NULL),
      type_(kGenericAddress) {
}

bool AddressField::ClassifyField(ServerFieldTypeMap* map) const {
  ServerFieldType address_company;
  ServerFieldType address_line1;
  ServerFieldType address_line2;
  ServerFieldType address_city;
  ServerFieldType address_state;
  ServerFieldType address_zip;
  ServerFieldType address_country;

  switch (type_) {
    case kShippingAddress:
     // Fall through. Autofill does not support shipping addresses.
    case kGenericAddress:
      address_company = COMPANY_NAME;
      address_line1 = ADDRESS_HOME_LINE1;
      address_line2 = ADDRESS_HOME_LINE2;
      address_city = ADDRESS_HOME_CITY;
      address_state = ADDRESS_HOME_STATE;
      address_zip = ADDRESS_HOME_ZIP;
      address_country = ADDRESS_HOME_COUNTRY;
      break;

    case kBillingAddress:
      address_company = COMPANY_NAME;
      address_line1 = ADDRESS_BILLING_LINE1;
      address_line2 = ADDRESS_BILLING_LINE2;
      address_city = ADDRESS_BILLING_CITY;
      address_state = ADDRESS_BILLING_STATE;
      address_zip = ADDRESS_BILLING_ZIP;
      address_country = ADDRESS_BILLING_COUNTRY;
      break;

    default:
      NOTREACHED();
      return false;
  }

  bool ok = AddClassification(company_, address_company, map);
  ok = ok && AddClassification(address1_, address_line1, map);
  ok = ok && AddClassification(address2_, address_line2, map);
  ok = ok && AddClassification(city_, address_city, map);
  ok = ok && AddClassification(state_, address_state, map);
  ok = ok && AddClassification(zip_, address_zip, map);
  ok = ok && AddClassification(country_, address_country, map);
  return ok;
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
  if (address1_)
    return false;

  base::string16 pattern = UTF8ToUTF16(autofill::kAddressLine1Re);
  base::string16 label_pattern = UTF8ToUTF16(autofill::kAddressLine1LabelRe);

  // TODO(isherman): Fill full address into textarea field...
  if (!ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT | MATCH_TEXT_AREA,
                           &address1_) &&
      !ParseFieldSpecifics(scanner, label_pattern,
                           MATCH_LABEL | MATCH_TEXT | MATCH_TEXT_AREA,
                           &address1_)) {
    return false;
  }

  // Optionally parse more address lines, which may have empty labels.
  // Some pages have 3 address lines (eg SharperImageModifyAccount.html)
  // Some pages even have 4 address lines (e.g. uk/ShoesDirect2.html)!
  pattern = UTF8ToUTF16(autofill::kAddressLine2Re);
  label_pattern = UTF8ToUTF16(autofill::kAddressLine2LabelRe);
  if (!ParseEmptyLabel(scanner, &address2_) &&
      !ParseField(scanner, pattern, &address2_)) {
    ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                        &address2_);
  }

  // Try for surplus lines, which we will promptly discard.
  if (address2_ != NULL) {
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
  //
  // HACK: Just for the MapQuest driving directions page we match the
  // exact name "1z", which MapQuest uses to label its zip code field.
  // Hopefully before long we'll be smart enough to find the zip code
  // on that page automatically.
  if (zip_)
    return false;

  base::string16 pattern = UTF8ToUTF16(autofill::kZipCodeRe);
  if (!ParseField(scanner, pattern, &zip_))
    return false;

  type_ = kGenericAddress;
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

// static
AddressField::AddressType AddressField::AddressTypeFromText(
    const base::string16& text) {
  std::string normalized_text = UTF16ToUTF8(StringToLowerASCII(text));

  size_t same_as = normalized_text.find(autofill::kAddressTypeSameAsRe);
  size_t use_shipping = normalized_text.find(autofill::kAddressTypeUseMyRe);
  if (same_as != base::string16::npos || use_shipping != base::string16::npos) {
    // This text could be a checkbox label such as "same as my billing
    // address" or "use my shipping address".
    // ++ It would help if we generally skipped all text that appears
    // after a check box.
    return kGenericAddress;
  }

  // Not all pages say "billing address" and "shipping address" explicitly;
  // for example, Craft Catalog1.html has "Bill-to Address" and
  // "Ship-to Address".
  size_t bill = normalized_text.rfind(autofill::kBillingDesignatorRe);
  size_t ship = normalized_text.rfind(autofill::kShippingDesignatorRe);

  if (bill == base::string16::npos && ship == base::string16::npos)
    return kGenericAddress;

  if (bill != base::string16::npos && ship == base::string16::npos)
    return kBillingAddress;

  if (bill == base::string16::npos && ship != base::string16::npos)
    return kShippingAddress;

  if (bill > ship)
    return kBillingAddress;

  return kShippingAddress;
}

}  // namespace autofill
