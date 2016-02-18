// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/parse_name.h"

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/third_party/icu/icu_utf.h"

namespace net {

namespace {

// Converts a BMPString value in Input |in| to UTF-8.
//
// If the conversion is successful, returns true and stores the result in
// |out|. Otherwise it returns false and leaves |out| unmodified.
bool ConvertBmpStringValue(const der::Input& in, std::string* out) {
  if (in.Length() % 2 != 0)
    return false;

  base::string16 in_16bit;
  if (in.Length()) {
    memcpy(base::WriteInto(&in_16bit, in.Length() / 2 + 1), in.UnsafeData(),
           in.Length());
  }
  for (base::char16& c : in_16bit) {
    // BMPString is UCS-2 in big-endian order.
    c = base::NetToHost16(c);

    // BMPString only supports codepoints in the Basic Multilingual Plane;
    // surrogates are not allowed.
    if (CBU_IS_SURROGATE(c))
      return false;
  }
  return base::UTF16ToUTF8(in_16bit.data(), in_16bit.size(), out);
}

// Converts a UniversalString value in Input |in| to UTF-8.
//
// If the conversion is successful, returns true and stores the result in
// |out|. Otherwise it returns false and leaves |out| unmodified.
bool ConvertUniversalStringValue(const der::Input& in, std::string* out) {
  if (in.Length() % 4 != 0)
    return false;

  std::vector<uint32_t> in_32bit(in.Length() / 4);
  if (in.Length())
    memcpy(in_32bit.data(), in.UnsafeData(), in.Length());
  for (const uint32_t c : in_32bit) {
    // UniversalString is UCS-4 in big-endian order.
    uint32_t codepoint = base::NetToHost32(c);
    if (!CBU_IS_UNICODE_CHAR(codepoint))
      return false;

    base::WriteUnicodeCharacter(codepoint, out);
  }
  return true;
}

}  // namespace

der::Input TypeCommonNameOid() {
  // id-at-commonName: 2.5.4.3 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x03};
  return der::Input(oid);
}

der::Input TypeSurnameOid() {
  // id-at-surname: 2.5.4.4 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x04};
  return der::Input(oid);
}

der::Input TypeSerialNumberOid() {
  // id-at-serialNumber: 2.5.4.5 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x05};
  return der::Input(oid);
}

der::Input TypeCountryNameOid() {
  // id-at-countryName: 2.5.4.6 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x06};
  return der::Input(oid);
}

der::Input TypeLocalityNameOid() {
  // id-at-localityName: 2.5.4.7 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x07};
  return der::Input(oid);
}

der::Input TypeStateOrProvinceNameOid() {
  // id-at-stateOrProvinceName: 2.5.4.8 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x08};
  return der::Input(oid);
}

der::Input TypeOrganizationNameOid() {
  // id-at-organizationName: 2.5.4.10 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x0a};
  return der::Input(oid);
}

der::Input TypeOrganizationUnitNameOid() {
  // id-at-organizationalUnitName: 2.5.4.11 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x0b};
  return der::Input(oid);
}

der::Input TypeTitleOid() {
  // id-at-title: 2.5.4.12 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x0c};
  return der::Input(oid);
}

der::Input TypeNameOid() {
  // id-at-name: 2.5.4.41 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x29};
  return der::Input(oid);
}

der::Input TypeGivenNameOid() {
  // id-at-givenName: 2.5.4.42 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x2a};
  return der::Input(oid);
}

der::Input TypeInitialsOid() {
  // id-at-initials: 2.5.4.43 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x2b};
  return der::Input(oid);
}

der::Input TypeGenerationQualifierOid() {
  // id-at-generationQualifier: 2.5.4.44 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x2c};
  return der::Input(oid);
}

bool X509NameAttribute::ValueAsStringUnsafe(std::string* out) const {
  switch (value_tag) {
    case der::kIA5String:
    case der::kPrintableString:
    case der::kTeletexString:
    case der::kUtf8String:
      *out = value.AsString();
      return true;
    case der::kUniversalString:
      return ConvertUniversalStringValue(value, out);
    case der::kBmpString:
      return ConvertBmpStringValue(value, out);
    default:
      NOTREACHED();
      return false;
  }
}

bool ReadRdn(der::Parser* parser, std::vector<X509NameAttribute>* out) {
  while (parser->HasMore()) {
    der::Parser attr_type_and_value;
    if (!parser->ReadSequence(&attr_type_and_value))
      return false;
    // Read the attribute type, which must be an OBJECT IDENTIFIER.
    der::Input type;
    if (!attr_type_and_value.ReadTag(der::kOid, &type))
      return false;

    // Read the attribute value.
    der::Tag tag;
    der::Input value;
    if (!attr_type_and_value.ReadTagAndValue(&tag, &value))
      return false;

    // There should be no more elements in the sequence after reading the
    // attribute type and value.
    if (attr_type_and_value.HasMore())
      return false;

    out->push_back(X509NameAttribute(type, tag, value));
  }

  // RFC 5280 section 4.1.2.4
  // RelativeDistinguishedName ::= SET SIZE (1..MAX) OF AttributeTypeAndValue
  return out->size() != 0;
}

bool ParseName(const der::Input& name_tlv,
               std::vector<X509NameAttribute>* out) {
  der::Parser name_parser(name_tlv);
  der::Parser rdn_sequence_parser;
  if (!name_parser.ReadSequence(&rdn_sequence_parser))
    return false;

  while (rdn_sequence_parser.HasMore()) {
    der::Parser rdn_parser;
    if (!rdn_sequence_parser.ReadConstructed(der::kSet, &rdn_parser))
      return false;
    std::vector<X509NameAttribute> type_and_values;
    if (!ReadRdn(&rdn_parser, &type_and_values))
      return false;
    for (const auto& type_and_value : type_and_values) {
      out->push_back(type_and_value);
    }
  }

  return true;
}

}  // namespace net
