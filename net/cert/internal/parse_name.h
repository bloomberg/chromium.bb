// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_PARSE_NAME_H_
#define NET_CERT_INTERNAL_PARSE_NAME_H_

#include <vector>

#include "net/base/net_export.h"
#include "net/der/input.h"
#include "net/der/parser.h"
#include "net/der/tag.h"

namespace net {

NET_EXPORT der::Input TypeCommonNameOid();
NET_EXPORT der::Input TypeSurnameOid();
NET_EXPORT der::Input TypeSerialNumberOid();
NET_EXPORT der::Input TypeCountryNameOid();
NET_EXPORT der::Input TypeLocalityNameOid();
NET_EXPORT der::Input TypeStateOrProvinceNameOid();
NET_EXPORT der::Input TypeOrganizationNameOid();
NET_EXPORT der::Input TypeOrganizationUnitNameOid();
NET_EXPORT der::Input TypeTitleOid();
NET_EXPORT der::Input TypeNameOid();
NET_EXPORT der::Input TypeGivenNameOid();
NET_EXPORT der::Input TypeInitialsOid();
NET_EXPORT der::Input TypeGenerationQualifierOid();

// X509NameAttribute contains a representation of a DER-encoded RFC 2253
// "AttributeTypeAndValue".
//
// AttributeTypeAndValue ::= SEQUENCE {
//     type  AttributeType,
//     value AttributeValue
// }
struct NET_EXPORT X509NameAttribute {
  X509NameAttribute(der::Input in_type,
                    der::Tag in_value_tag,
                    der::Input in_value)
      : type(in_type), value_tag(in_value_tag), value(in_value) {}

  // Attempts to convert the value represented by this struct into a
  // std::string and store it in |out|, returning whether the conversion was
  // successful. Due to some encodings being incompatible, the caller must
  // verify the attribute |type|.
  //
  // Note: The conversion doesn't verify that the value corresponds to the
  // ASN.1 definition of the value type.
  bool ValueAsStringUnsafe(std::string* out) const WARN_UNUSED_RESULT;

  der::Input type;
  der::Tag value_tag;
  der::Input value;
};

// Parses all the ASN.1 AttributeTypeAndValue elements in |parser| and stores
// each as an AttributeTypeAndValue object in |out|.
//
// AttributeTypeAndValue is defined in RFC 5280 section 4.1.2.4:
//
// AttributeTypeAndValue ::= SEQUENCE {
//   type     AttributeType,
//   value    AttributeValue }
//
// AttributeType ::= OBJECT IDENTIFIER
//
// AttributeValue ::= ANY -- DEFINED BY AttributeType
//
// DirectoryString ::= CHOICE {
//       teletexString           TeletexString (SIZE (1..MAX)),
//       printableString         PrintableString (SIZE (1..MAX)),
//       universalString         UniversalString (SIZE (1..MAX)),
//       utf8String              UTF8String (SIZE (1..MAX)),
//       bmpString               BMPString (SIZE (1..MAX)) }
//
// The type of the component AttributeValue is determined by the AttributeType;
// in general it will be a DirectoryString.
NET_EXPORT bool ReadRdn(der::Parser* parser,
                        std::vector<X509NameAttribute>* out) WARN_UNUSED_RESULT;

// Parses a DER-encoded "Name" as specified by 5280. Returns true on success
// and sets the results in |out|.
NET_EXPORT bool ParseName(const der::Input& name_tlv,
                          std::vector<X509NameAttribute>* out)
    WARN_UNUSED_RESULT;

}  // namespace net

#endif  // NET_CERT_INTERNAL_PARSE_NAME_H_
