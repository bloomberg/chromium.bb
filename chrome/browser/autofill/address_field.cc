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
#include "chrome/browser/autofill/autofill_scanner.h"
#include "chrome/browser/autofill/field_types.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kAttentionIgnoredRe[] = "attention|attn";
const char kRegionIgnoredRe[] =
    "province|region|other"
    // es
    "|provincia"
      // pt-BR, pt-PT
    "|bairro|suburb";
const char kCompanyRe[] =
    "company|business|organization|organisation"
    // de-DE
    "|firma|firmenname"
    // es
    "|empresa"
    // fr-FR: |societe|société
    "|societe|soci\xc3\xa9t\xc3\xa9"
    // it-IT
    "|ragione.?sociale"
    // ja-JP: 会社
    "|\xe4\xbc\x9a\xe7\xa4\xbe"
    // ru: название.?компании
    "|\xd0\xbd\xd0\xb0\xd0\xb7\xd0\xb2\xd0\xb0\xd0\xbd\xd0\xb8\xd0\xb5.?\xd0"
        "\xba\xd0\xbe\xd0\xbc\xd0\xbf\xd0\xb0\xd0\xbd\xd0\xb8\xd0\xb8"
    // zh-CN: 单位|公司
    "|\xe5\x8d\x95\xe4\xbd\x8d|\xe5\x85\xac\xe5\x8f\xb8"
    // ko-KR: 회사|직장
    "|\xed\x9a\x8c\xec\x82\xac|\xec\xa7\x81\xec\x9e\xa5";
const char kAddressLine1Re[] =
    "address.*line|address1|addr1|street"
    // de-DE: |strasse|straße|hausnummer|housenumber
    "|strasse|stra\xc3\x9f""e|hausnummer|housenumber"
    // en-GB
    "|house.?name"
    // es: |direccion|dirección
    "|direccion|direcci\xc3\xb3n"
    // fr-FR
    "|adresse"
    // it-IT
    "|indirizzo"
    // ja-JP: 住所1
    "|\xe4\xbd\x8f\xe6\x89\x80""1"
    // pt-BR, pt-PT: morada|endereço
    "|morada|endere\xc3\xa7o"
    // ru: Адрес
    "|\xd0\x90\xd0\xb4\xd1\x80\xd0\xb5\xd1\x81"
    // zh-CN: 地址
    "|\xe5\x9c\xb0\xe5\x9d\x80"
    // ko-KR: 주소.?1
    "|\xec\xa3\xbc\xec\x86\x8c.?1";
const char kAddressLine1LabelRe[] =
    "address"
    // fr-FR: |adresse
    "|adresse"
    // it-IT: |indirizzo
    "|indirizzo"
    // ja-JP: |住所
    "|\xe4\xbd\x8f\xe6\x89\x80"
    // zh-CN: |地址
    "|\xe5\x9c\xb0\xe5\x9d\x80"
    // ko-KR: |주소
    "|\xec\xa3\xbc\xec\x86\x8c";
const char kAddressLine2Re[] =
    "address.*line2|address2|addr2|street|suite|unit"
    // de-DE: |adresszusatz|ergänzende.?angaben
    "|adresszusatz|erg\xc3\xa4nzende.?angaben"
    // es: |direccion2|colonia|adicional
    "|direccion2|colonia|adicional"
    // fr-FR: |addresssuppl|complementnom|appartement
    "|addresssuppl|complementnom|appartement"
    // it-IT: |indirizzo2
    "|indirizzo2"
    // ja-JP: |住所2
    "|\xe4\xbd\x8f\xe6\x89\x80""2"
    // pt-BR, pt-PT: |complemento|addrcomplement
    "|complemento|addrcomplement"
    // ru: |Улица
    "|\xd0\xa3\xd0\xbb\xd0\xb8\xd1\x86\xd0\xb0"
    // zh-CN: |地址2
    "|\xe5\x9c\xb0\xe5\x9d\x80""2"
    // ko-KR: |주소.?2
    "|\xec\xa3\xbc\xec\x86\x8c.?2";
const char kAddressLine2LabelRe[] =
    "address"
    // fr-FR: |adresse
    "|adresse"
    // it-IT: |indirizzo
    "|indirizzo"
    // zh-CN: |地址
    "|\xe5\x9c\xb0\xe5\x9d\x80"
    // ko-KR: |주소
    "|\xec\xa3\xbc\xec\x86\x8c";
const char kAddressLine3Re[] =
    "address.*line3|address3|addr3|street|line3"
    // es: |municipio
    "|municipio"
    // fr-FR: |batiment|residence
    "|batiment|residence"
    // it-IT: |indirizzo3
    "|indirizzo3";
const char kCountryRe[] =
    "country|countries|location"
    // es: |país|pais
    "|pa\xc3\xads|pais"
    // ja-JP: |国
    "|\xe5\x9b\xbd"
    // zh-CN: |国家
    "|\xe5\x9b\xbd\xe5\xae\xb6"
    // ko-KR: |국가|나라
    "|\xea\xb5\xad\xea\xb0\x80|\xeb\x82\x98\xeb\x9d\xbc";
const char kZipCodeRe[] =
    "zip|postal|post.*code|pcode|^1z$"
    // de-DE: |postleitzahl
    "|postleitzahl"
    // es: |\bcp\b
    "|\\bcp\\b"
    // fr-FR: |\bcdp\b
    "|\\bcdp\\b"
    // it-IT: |\bcap\b
    "|\\bcap\\b"
    // ja-JP: |郵便番号
    "|\xe9\x83\xb5\xe4\xbe\xbf\xe7\x95\xaa\xe5\x8f\xb7"
    // pt-BR, pt-PT: |codigo|codpos|\bcep\b
    "|codigo|codpos|\\bcep\\b"
    // ru: |Почтовый.?Индекс
    "|\xd0\x9f\xd0\xbe\xd1\x87\xd1\x82\xd0\xbe\xd0\xb2\xd1\x8b\xd0\xb9.?\xd0"
        "\x98\xd0\xbd\xd0\xb4\xd0\xb5\xd0\xba\xd1\x81"
    // zh-CN: |邮政编码|邮编
    "|\xe9\x82\xae\xe6\x94\xbf\xe7\xbc\x96\xe7\xa0\x81|\xe9\x82\xae\xe7\xbc"
        "\x96"
    // zh-TW: |郵遞區號
    "|\xe9\x83\xb5\xe9\x81\x9e\xe5\x8d\x80\xe8\x99\x9f"
    // ko-KR: |우편.?번호
    "|\xec\x9a\xb0\xed\x8e\xb8.?\xeb\xb2\x88\xed\x98\xb8";
const char kZip4Re[] =
    "zip|^-$|post2"
    // pt-BR, pt-PT: |codpos2
    "|codpos2";
const char kCityRe[] =
    "city|town"
    // de-DE: |\bort\b|stadt
    "|\\bort\\b|stadt"
    // en-AU: |suburb
    "|suburb"
    // es: |ciudad|provincia|localidad|poblacion
    "|ciudad|provincia|localidad|poblacion"
    // fr-FR: |ville|commune
    "|ville|commune"
    // it-IT: |localita
    "|localita"
    // ja-JP: |市区町村
    "|\xe5\xb8\x82\xe5\x8c\xba\xe7\x94\xba\xe6\x9d\x91"
    // pt-BR, pt-PT: |cidade
    "|cidade"
    // ru: |Город
    "|\xd0\x93\xd0\xbe\xd1\x80\xd0\xbe\xd0\xb4"
    // zh-CN: |市
    "|\xe5\xb8\x82"
    // zh-TW: |分區
    "|\xe5\x88\x86\xe5\x8d\x80"
    // ko-KR: |^시[^도·・]|시[·・]?군[·・]?구
    "|^\xec\x8b\x9c[^\xeb\x8f\x84\xc2\xb7\xe3\x83\xbb]|\xec\x8b\x9c[\xc2\xb7"
        "\xe3\x83\xbb]?\xea\xb5\xb0[\xc2\xb7\xe3\x83\xbb]?\xea\xb5\xac";
const char kStateRe[] =
    "(?<!united )state|county|region|province"
    // de-DE: |land
    "|land"
    // en-UK: |county|principality
    "|county|principality"
    // ja-JP: |都道府県
    "|\xe9\x83\xbd\xe9\x81\x93\xe5\xba\x9c\xe7\x9c\x8c"
    // pt-BR, pt-PT: |estado|provincia
    "|estado|provincia"
    // ru: |область
    "|\xd0\xbe\xd0\xb1\xd0\xbb\xd0\xb0\xd1\x81\xd1\x82\xd1\x8c"
    // zh-CN: |省
    "|\xe7\x9c\x81"
    // zh-TW: |地區
    "|\xe5\x9c\xb0\xe5\x8d\x80"
    // ko-KR: |^시[·・]?도
    "|^\xec\x8b\x9c[\xc2\xb7\xe3\x83\xbb]?\xeb\x8f\x84";
const char kAddressTypeSameAsRe[] = "same as";
const char kAddressTypeUseMyRe[] = "use my";
const char kBillingDesignatorRe[] = "bill";
const char kShippingDesignatorRe[] = "ship";

}  // namespace

FormField* AddressField::Parse(AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return NULL;

  scoped_ptr<AddressField> address_field(new AddressField);
  const AutofillField* const initial_field = scanner->Cursor();
  size_t saved_cursor = scanner->SaveCursor();

  string16 attention_ignored = UTF8ToUTF16(kAttentionIgnoredRe);
  string16 region_ignored = UTF8ToUTF16(kRegionIgnoredRe);

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

  return ParseField(scanner, UTF8ToUTF16(kCompanyRe), &address_field->company_);
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

  string16 pattern = UTF8ToUTF16(kAddressLine1Re);
  string16 label_pattern = UTF8ToUTF16(kAddressLine1LabelRe);

  if (!ParseField(scanner, pattern, &address_field->address1_) &&
      !ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                           &address_field->address1_)) {
    return false;
  }

  // Optionally parse more address lines, which may have empty labels.
  // Some pages have 3 address lines (eg SharperImageModifyAccount.html)
  // Some pages even have 4 address lines (e.g. uk/ShoesDirect2.html)!
  pattern = UTF8ToUTF16(kAddressLine2Re);
  label_pattern = UTF8ToUTF16(kAddressLine2LabelRe);
  if (!ParseEmptyLabel(scanner, &address_field->address2_) &&
      !ParseField(scanner, pattern, &address_field->address2_)) {
    ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                        &address_field->address2_);
  }

  // Try for a third line, which we will promptly discard.
  if (address_field->address2_ != NULL) {
    pattern = UTF8ToUTF16(kAddressLine3Re);
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
                             UTF8ToUTF16(kCountryRe),
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

  string16 pattern = UTF8ToUTF16(kZipCodeRe);
  if (!ParseField(scanner, pattern, &address_field->zip_))
    return false;

  address_field->type_ = kGenericAddress;
  // Look for a zip+4, whose field name will also often contain
  // the substring "zip".
  ParseField(scanner,
             UTF8ToUTF16(kZip4Re),
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
                             UTF8ToUTF16(kCityRe),
                             MATCH_DEFAULT | MATCH_SELECT,
                             &address_field->city_);
}

// static
bool AddressField::ParseState(AutofillScanner* scanner,
                              AddressField* address_field) {
  if (address_field->state_)
    return false;

  return ParseFieldSpecifics(scanner,
                             UTF8ToUTF16(kStateRe),
                             MATCH_DEFAULT | MATCH_SELECT,
                             &address_field->state_);
}

AddressField::AddressType AddressField::AddressTypeFromText(
    const string16 &text) {
  if (text.find(UTF8ToUTF16(kAddressTypeSameAsRe)) != string16::npos ||
      text.find(UTF8ToUTF16(kAddressTypeUseMyRe)) != string16::npos)
    // This text could be a checkbox label such as "same as my billing
    // address" or "use my shipping address".
    // ++ It would help if we generally skipped all text that appears
    // after a check box.
    return kGenericAddress;

  // Not all pages say "billing address" and "shipping address" explicitly;
  // for example, Craft Catalog1.html has "Bill-to Address" and
  // "Ship-to Address".
  size_t bill = text.rfind(UTF8ToUTF16(kBillingDesignatorRe));
  size_t ship = text.rfind(UTF8ToUTF16(kShippingDesignatorRe));

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
