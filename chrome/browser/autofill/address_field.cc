// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/address_field.h"

#include "base/logging.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/autofill_field.h"

bool AddressField::GetFieldInfo(FieldTypeMap* field_type_map) const {
  AutoFillFieldType address_line1;
  AutoFillFieldType address_line2;
  AutoFillFieldType address_appt_num;
  AutoFillFieldType address_city;
  AutoFillFieldType address_state;
  AutoFillFieldType address_zip;
  AutoFillFieldType address_country;

  switch (type_) {
    case kShippingAddress:
     // Fallthrough. Autofill no longer supports shipping addresses.
    case kGenericAddress:
      address_line1 = ADDRESS_HOME_LINE1;
      address_line2 = ADDRESS_HOME_LINE2;
      address_appt_num = ADDRESS_HOME_APPT_NUM;
      address_city = ADDRESS_HOME_CITY;
      address_state = ADDRESS_HOME_STATE;
      address_zip = ADDRESS_HOME_ZIP;
      address_country = ADDRESS_HOME_COUNTRY;
      break;

    case kBillingAddress:
      address_line1 = ADDRESS_BILLING_LINE1;
      address_line2 = ADDRESS_BILLING_LINE2;
      address_appt_num = ADDRESS_BILLING_APPT_NUM;
      address_city = ADDRESS_BILLING_CITY;
      address_state = ADDRESS_BILLING_STATE;
      address_zip = ADDRESS_BILLING_ZIP;
      address_country = ADDRESS_BILLING_COUNTRY;
      break;

    default:
      NOTREACHED();
      return false;
  }

  bool ok;
  ok = Add(field_type_map, address1_, AutoFillType(address_line1));
  DCHECK(ok);
  ok = ok && Add(field_type_map, address2_, AutoFillType(address_line2));
  DCHECK(ok);
  ok = ok && Add(field_type_map, city_, AutoFillType(address_city));
  DCHECK(ok);
  ok = ok && Add(field_type_map, state_, AutoFillType(address_state));
  DCHECK(ok);
  ok = ok && Add(field_type_map, zip_, AutoFillType(address_zip));
  DCHECK(ok);
  ok = ok && Add(field_type_map, country_, AutoFillType(address_country));
  DCHECK(ok);

  return ok;
}

AddressField* AddressField::Parse(
    std::vector<AutoFillField*>::const_iterator* iter,
    bool is_ecml) {
  AddressField address_field;
  std::vector<AutoFillField*>::const_iterator q = *iter;
  string16 pattern;

  // The ECML standard uses 2 letter country codes.  So we will
  // have to remember that this is an ECML form, for when we fill
  // it out.
  address_field.is_ecml_ = is_ecml;

  // Allow address fields to appear in any order.
  do {
    if (ParseText(&q, ASCIIToUTF16("company|business name")))
      continue;  // Ignore company name for now.

    // Some pages (e.g. PC Connection.html) have an Attention field inside an
    // address; we ignore this for now.
    if (ParseText(&q, ASCIIToUTF16("attention|attn.")))
      continue;

    if (ParseAddressLines(&q, is_ecml, &address_field))
      continue;

    if (ParseCity(&q, is_ecml, &address_field))
      continue;

    if ((!address_field.state_ || address_field.state_->IsEmpty()) &&
        address_field.ParseState(&q)) {
      continue;
    }

    if (ParseZipCode(&q, is_ecml, &address_field))
      continue;

    if (ParseCountry(&q, is_ecml, &address_field))
      continue;

    // Some test pages (e.g. SharperImageModifyAccount.html,
    // Craft Catalog1.html, FAO Schwarz Billing Info Page.html) have a
    // "province"/"region"/"other" field; we ignore this field for now.
    if (ParseText(&q, ASCIIToUTF16("province|region|other")))
      continue;

    // Ignore non-labeled fields within an address; the page
    // MapQuest Driving Directions North America.html contains such a field.
    // We only ignore such fields after we've parsed at least one other field;
    // otherwise we'd effectively parse address fields before other field types
    // after any non-labeled fields, and we want email address fields to have
    // precedence since some pages contain fields labeled "Email address".
  } while (q != *iter && ParseEmpty(&q));

  // If we have identified any address fields in this field then it should be
  // added to the list of fields.
  if (address_field.address1_ != NULL || address_field.address2_ != NULL ||
      address_field.city_ != NULL || address_field.state_ != NULL ||
      address_field.zip_ != NULL || address_field.zip4_ ||
      address_field.country_ != NULL) {
    *iter = q;
    return new AddressField(address_field);
  }

  return NULL;
}

AddressType AddressField::FindType() const {
  // This is not a full address, so don't even bother trying to figure
  // out its type.
  if (address1_ == NULL)
    return kGenericAddress;

  // First look at the field name, which itself will sometimes contain
  // "bill" or "ship".  We could check for the ECML type prefixes
  // here, but there's no need to since ECML's prefixes Ecom_BillTo
  // and Ecom_ShipTo contain "bill" and "ship" anyway.
  string16 name = StringToLowerASCII(address1_->name());
  AddressType address_type = AddressTypeFromText(name);
  if (address_type)
    return address_type;

  // TODO(jhawkins): Look at table cells above this point.
  return kGenericAddress;
}

AddressField::AddressField()
    : address1_(NULL),
      address2_(NULL),
      city_(NULL),
      state_(NULL),
      zip_(NULL),
      zip4_(NULL),
      country_(NULL),
      type_(kGenericAddress),
      is_ecml_(false) {
}

AddressField::AddressField(const AddressField& field)
    : address1_(field.address1_),
      address2_(field.address2_),
      city_(field.city_),
      state_(field.state_),
      zip_(field.zip_),
      zip4_(field.zip4_),
      country_(field.country_),
      type_(field.type_),
      is_ecml_(field.is_ecml_) {
}

// static
bool AddressField::ParseAddressLines(
    std::vector<AutoFillField*>::const_iterator* iter,
    bool is_ecml, AddressField* address_field) {
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

  string16 pattern;
  if (is_ecml) {
    pattern = GetEcmlPattern(kEcmlShipToAddress1,
                             kEcmlBillToAddress1, '|');
  } else {
    pattern =
        ASCIIToUTF16("@address|street|address line|address1|street_line1");
  }

  if (!ParseText(iter, pattern, &address_field->address1_))
    return false;

  // Some pages (e.g. expedia_checkout.html) have an apartment or
  // suite number at this point.  The occasional page (e.g.
  // Ticketmaster3.html) calls this a unit number.  We ignore this
  // field since we can't fill it yet.
  ParseText(iter, ASCIIToUTF16("suite|unit"));

  // Optionally parse more address lines, which may have empty labels.
  // Some pages have 3 address lines (eg SharperImageModifyAccount.html)
  // Some pages even have 4 address lines (e.g. uk/ShoesDirect2.html)!
  if (is_ecml) {
    pattern = GetEcmlPattern(kEcmlShipToAddress2,
                             kEcmlBillToAddress2, '|');
  } else {
    pattern = ASCIIToUTF16("|address|address2|street|street_line2");
  }

  ParseText(iter, pattern, &address_field->address2_);
  return true;
}

// static
bool AddressField::ParseCountry(
    std::vector<AutoFillField*>::const_iterator* iter,
    bool is_ecml, AddressField* address_field) {
  // Parse a country.  The occasional page (e.g.
  // Travelocity_New Member Information1.html) calls this a "location".
  // Note: ECML standard uses 2 letter country code (ISO 3166)
  if (address_field->country_ && !address_field->country_->IsEmpty())
    return false;

  // TODO(jhawkins): Parse the country.
  return false;
}

// static
bool AddressField::ParseZipCode(
    std::vector<AutoFillField*>::const_iterator* iter,
    bool is_ecml, AddressField* address_field) {
  // Parse a zip code.  On some UK pages (e.g. The China Shop2.html) this
  // is called a "post code".
  //
  // HACK: Just for the MapQuest driving directions page we match the
  // exact name "1z", which MapQuest uses to label its zip code field.
  // Hopefully before long we'll be smart enough to find the zip code
  // on that page automatically.
  if (address_field->zip_)
    return false;

  string16 pattern;
  if (is_ecml) {
    pattern = GetEcmlPattern(kEcmlShipToPostalCode,
                             kEcmlBillToPostalCode, '|');
  } else {
    pattern = ASCIIToUTF16("zip|postal|post code|^1z");
  }

  AddressType tempType;
  string16 name = (**iter)->name();

  // Note: comparisons using the ecml compliant name as a prefix must be used in
  // order to accommodate Google Checkout. See FormFieldSet::GetEcmlPattern for
  // more detail.
  if (StartsWith(name, kEcmlBillToPostalCode, false)) {
    tempType = kBillingAddress;
  } else if (StartsWith(name, kEcmlShipToPostalCode, false)) {
    tempType = kShippingAddress;
  } else {
    tempType = kGenericAddress;
  }

  if (!ParseText(iter, pattern, &address_field->zip_))
    return false;

  address_field->type_ = tempType;
  if (!is_ecml) {
    // Look for a zip+4, whose field name will also often contain
    // the substring "zip".
    ParseText(iter, ASCIIToUTF16("zip|^-"), &address_field->zip4_);
  }

  return true;
}

// static
bool AddressField::ParseCity(
    std::vector<AutoFillField*>::const_iterator* iter,
    bool is_ecml, AddressField* address_field) {
  // Parse a city name.  Some UK pages (e.g. The China Shop2.html) use
  // the term "town".
  if (address_field->city_)
    return false;

  string16 pattern;
  if (is_ecml)
    pattern = GetEcmlPattern(kEcmlShipToCity, kEcmlBillToCity, '|');
  else
    pattern = ASCIIToUTF16("city|town");

  if (!ParseText(iter, pattern, &address_field->city_))
    return false;

  return true;
}

bool AddressField::ParseState(
    std::vector<AutoFillField*>::const_iterator* iter) {
  // TODO(jhawkins): Parse the state.
  return false;
}

AddressType AddressField::AddressTypeFromText(const string16 &text) {
  if (text.find(ASCIIToUTF16("same as")) != string16::npos ||
      text.find(ASCIIToUTF16("use my")) != string16::npos)
    // This text could be a checkbox label such as "same as my billing
    // address" or "use my shipping address".
    // ++ It would help if we generally skipped all text that appears
    // after a check box.
    return kGenericAddress;

  // Not all pages say "billing address" and "shipping address" explicitly;
  // for example, Craft Catalog1.html has "Bill-to Address" and
  // "Ship-to Address".
  size_t bill = text.find_last_of(ASCIIToUTF16("bill"));
  size_t ship = text.find_last_of(ASCIIToUTF16("ship"));

  if (bill != string16::npos && bill > ship)
    return kBillingAddress;

  if (ship != string16::npos)
    return kShippingAddress;

  return kGenericAddress;
}
