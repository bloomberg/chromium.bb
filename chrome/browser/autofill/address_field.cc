// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/address_field.h"

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

FormField* AddressField::Parse(AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return NULL;

  scoped_ptr<AddressField> address_field(new AddressField);
  const AutofillField* const initial_field = scanner->Cursor();
  size_t saved_cursor = scanner->SaveCursor();

  string16 attention_ignored = UTF8ToUTF16(autofill::kAttentionIgnoredRe);
  string16 region_ignored = UTF8ToUTF16(autofill::kRegionIgnoredRe);

  // Allow address fields to appear in any order.
  size_t begin_trailing_non_labeled_fields = 0;
  bool has_trailing_non_labeled_fields = false;
  while (!scanner->IsEnd()) {
    const size_t cursor = scanner->SaveCursor();
    if (ParseAddressLines(scanner, address_field.get()) ||
        ParseCity(scanner, address_field.get()) ||
        ParseState(scanner, address_field.get()) ||
        ParseZipCode(scanner, address_field.get()) ||
        ParseCountry(scanner, address_field.get()) ||
        ParseCompany(scanner, address_field.get())) {
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
  if (company_) {
    string16 name = StringToLowerASCII(company_->name);
    return AddressTypeFromText(name);
  }
  if (address1_) {
    string16 name = StringToLowerASCII(address1_->name);
    return AddressTypeFromText(name);
  }
  if (address2_) {
    string16 name = StringToLowerASCII(address2_->name);
    return AddressTypeFromText(name);
  }
  if (city_) {
    string16 name = StringToLowerASCII(city_->name);
    return AddressTypeFromText(name);
  }
  if (zip_) {
    string16 name = StringToLowerASCII(zip_->name);
    return AddressTypeFromText(name);
  }
  if (state_) {
    string16 name = StringToLowerASCII(state_->name);
    return AddressTypeFromText(name);
  }
  if (country_) {
    string16 name = StringToLowerASCII(country_->name);
    return AddressTypeFromText(name);
  }

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

bool AddressField::ClassifyField(FieldTypeMap* map) const {
  AutofillFieldType address_company;
  AutofillFieldType address_line1;
  AutofillFieldType address_line2;
  AutofillFieldType address_city;
  AutofillFieldType address_state;
  AutofillFieldType address_zip;
  AutofillFieldType address_country;

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

// static
bool AddressField::ParseCompany(AutofillScanner* scanner,
                                AddressField* address_field) {
  if (address_field->company_ && !address_field->company_->IsEmpty())
    return false;

  return ParseField(scanner, UTF8ToUTF16(autofill::kCompanyRe),
                    &address_field->company_);
}

// static
bool AddressField::ParseAddressLines(AutofillScanner* scanner,
                                     AddressField* address_field) {
  // We only match the string "address" in page text, not in element names,
  // because sometimes every element in a group of address fields will have
  // a name containing the string "address"; for example, on the page
  // Kohl's - Register Billing Address.html the text element labeled "city"
  // has the name "BILL_TO_ADDRESS<>city".  We do match address labels
  // such as "address1", which appear as element names on various pages (eg
  // AmericanGirl-Registration.html, BloomingdalesBilling.html,
  // EBay Registration Enter Information.html).
  if (address_field->address1_)
    return false;

  string16 pattern = UTF8ToUTF16(autofill::kAddressLine1Re);
  string16 label_pattern = UTF8ToUTF16(autofill::kAddressLine1LabelRe);

  if (!ParseField(scanner, pattern, &address_field->address1_) &&
      !ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                           &address_field->address1_)) {
    return false;
  }

  // Optionally parse more address lines, which may have empty labels.
  // Some pages have 3 address lines (eg SharperImageModifyAccount.html)
  // Some pages even have 4 address lines (e.g. uk/ShoesDirect2.html)!
  pattern = UTF8ToUTF16(autofill::kAddressLine2Re);
  label_pattern = UTF8ToUTF16(autofill::kAddressLine2LabelRe);
  if (!ParseEmptyLabel(scanner, &address_field->address2_) &&
      !ParseField(scanner, pattern, &address_field->address2_)) {
    ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                        &address_field->address2_);
  }

  // Try for a third line, which we will promptly discard.
  if (address_field->address2_ != NULL) {
    pattern = UTF8ToUTF16(autofill::kAddressLine3Re);
    ParseField(scanner, pattern, NULL);
  }

  return true;
}

// static
bool AddressField::ParseCountry(AutofillScanner* scanner,
                                AddressField* address_field) {
  // Parse a country.  The occasional page (e.g.
  // Travelocity_New Member Information1.html) calls this a "location".
  if (address_field->country_ && !address_field->country_->IsEmpty())
    return false;

  return ParseFieldSpecifics(scanner,
                             UTF8ToUTF16(autofill::kCountryRe),
                             MATCH_DEFAULT | MATCH_SELECT,
                             &address_field->country_);
}

// static
bool AddressField::ParseZipCode(AutofillScanner* scanner,
                                AddressField* address_field) {
  // Parse a zip code.  On some UK pages (e.g. The China Shop2.html) this
  // is called a "post code".
  //
  // HACK: Just for the MapQuest driving directions page we match the
  // exact name "1z", which MapQuest uses to label its zip code field.
  // Hopefully before long we'll be smart enough to find the zip code
  // on that page automatically.
  if (address_field->zip_)
    return false;

  string16 pattern = UTF8ToUTF16(autofill::kZipCodeRe);
  if (!ParseField(scanner, pattern, &address_field->zip_))
    return false;

  address_field->type_ = kGenericAddress;
  // Look for a zip+4, whose field name will also often contain
  // the substring "zip".
  ParseField(scanner,
             UTF8ToUTF16(autofill::kZip4Re),
             &address_field->zip4_);

  return true;
}

// static
bool AddressField::ParseCity(AutofillScanner* scanner,
                             AddressField* address_field) {
  // Parse a city name.  Some UK pages (e.g. The China Shop2.html) use
  // the term "town".
  if (address_field->city_)
    return false;

  // Select fields are allowed here.  This occurs on top-100 site rediff.com.
  return ParseFieldSpecifics(scanner,
                             UTF8ToUTF16(autofill::kCityRe),
                             MATCH_DEFAULT | MATCH_SELECT,
                             &address_field->city_);
}

// static
bool AddressField::ParseState(AutofillScanner* scanner,
                              AddressField* address_field) {
  if (address_field->state_)
    return false;

  return ParseFieldSpecifics(scanner,
                             UTF8ToUTF16(autofill::kStateRe),
                             MATCH_DEFAULT | MATCH_SELECT,
                             &address_field->state_);
}

AddressField::AddressType AddressField::AddressTypeFromText(
    const string16 &text) {
  size_t same_as = text.find(UTF8ToUTF16(autofill::kAddressTypeSameAsRe));
  size_t use_shipping = text.find(UTF8ToUTF16(autofill::kAddressTypeUseMyRe));
  if (same_as != string16::npos || use_shipping != string16::npos)
    // This text could be a checkbox label such as "same as my billing
    // address" or "use my shipping address".
    // ++ It would help if we generally skipped all text that appears
    // after a check box.
    return kGenericAddress;

  // Not all pages say "billing address" and "shipping address" explicitly;
  // for example, Craft Catalog1.html has "Bill-to Address" and
  // "Ship-to Address".
  size_t bill = text.rfind(UTF8ToUTF16(autofill::kBillingDesignatorRe));
  size_t ship = text.rfind(UTF8ToUTF16(autofill::kShippingDesignatorRe));

  if (bill == string16::npos && ship == string16::npos)
    return kGenericAddress;

  if (bill != string16::npos && ship == string16::npos)
    return kBillingAddress;

  if (bill == string16::npos && ship != string16::npos)
    return kShippingAddress;

  if (bill > ship)
    return kBillingAddress;

  return kShippingAddress;
}
