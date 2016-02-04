// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/verify_name_match.h"

#include <string.h>

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/tuple.h"
#include "net/der/input.h"
#include "net/der/parser.h"
#include "net/der/tag.h"

namespace net {

namespace {

// RFC 5280 section A.1:
//
// pkcs-9 OBJECT IDENTIFIER ::=
//   { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) 9 }
//
// id-emailAddress      AttributeType ::= { pkcs-9 1 }
//
// In dotted form: 1.2.840.113549.1.9.1
const uint8_t kOidEmailAddress[] = {0x2A, 0x86, 0x48, 0x86, 0xF7,
                                    0x0D, 0x01, 0x09, 0x01};

// Types of character set checking that NormalizeDirectoryString can perform.
enum CharsetEnforcement {
  NO_ENFORCEMENT,
  ENFORCE_PRINTABLE_STRING,
  ENFORCE_ASCII,
};

// Normalizes |output|, a UTF-8 encoded string, as if it contained
// only ASCII characters.
//
// This could be considered a partial subset of RFC 5280 rules, and
// is compatible with RFC 2459/3280.
//
// In particular, RFC 5280, Section 7.1 describes how UTF8String
// and PrintableString should be compared - using the LDAP StringPrep
// profile of RFC 4518, with case folding and whitespace compression.
// However, because it is optional for 2459/3280 implementations and because
// it's desirable to avoid the size cost of the StringPrep tables,
// this function treats |output| as if it was composed of ASCII.
//
// That is, rather than folding all whitespace characters, it only
// folds ' '. Rather than case folding using locale-aware handling,
// it only folds A-Z to a-z.
//
// This gives better results than outright rejecting (due to mismatched
// encodings), or from doing a strict binary comparison (the minimum
// required by RFC 3280), and is sufficient for those certificates
// publicly deployed.
//
// If |charset_enforcement| is not NO_ENFORCEMENT and |output| contains any
// characters not allowed in the specified charset, returns false.
//
// NOTE: |output| will be modified regardless of the return.
WARN_UNUSED_RESULT bool NormalizeDirectoryString(
    CharsetEnforcement charset_enforcement,
    std::string* output) {
  // Normalized version will always be equal or shorter than input.
  // Normalize in place and then truncate the output if necessary.
  std::string::const_iterator read_iter = output->begin();
  std::string::iterator write_iter = output->begin();

  for (; read_iter != output->end() && *read_iter == ' '; ++read_iter) {
    // Ignore leading whitespace.
  }

  for (; read_iter != output->end(); ++read_iter) {
    const unsigned char c = *read_iter;
    if (c == ' ') {
      // If there are non-whitespace characters remaining in input, compress
      // multiple whitespace chars to a single space, otherwise ignore trailing
      // whitespace.
      std::string::const_iterator next_iter = read_iter + 1;
      if (next_iter != output->end() && *next_iter != ' ')
        *(write_iter++) = ' ';
    } else if (c >= 'A' && c <= 'Z') {
      // Fold case.
      *(write_iter++) = c + ('a' - 'A');
    } else {
      // Note that these checks depend on the characters allowed by earlier
      // conditions also being valid for the enforced charset.
      switch (charset_enforcement) {
        case ENFORCE_PRINTABLE_STRING:
          // See NormalizePrintableStringValue comment for the acceptable list
          // of characters.
          if (!((c >= 'a' && c <= 'z') || (c >= '\'' && c <= ':') || c == '=' ||
                c == '?'))
            return false;
          break;
        case ENFORCE_ASCII:
          if (c > 0x7F)
            return false;
          break;
        case NO_ENFORCEMENT:
          break;
      }
      *(write_iter++) = c;
    }
  }
  if (write_iter != output->end())
    output->erase(write_iter, output->end());
  return true;
}

// Normalizes the DER-encoded PrintableString value |in| according to
// RFC 2459, Section 4.1.2.4
//
// Briefly, normalization involves removing leading and trailing
// whitespace, folding multiple whitespace characters into a single
// whitespace character, and normalizing on case (this function
// normalizes to lowercase).
//
// During normalization, this function also validates that |in|
// is properly encoded - that is, that it restricts to the character
// set defined in X.680 (2008), Section 41.4, Table 10. X.680 defines
// the valid characters as
//   a-z A-Z 0-9 (space) ' ( ) + , - . / : = ?
//
// However, due to an old OpenSSL encoding bug, a number of
// certificates have also included '*', which has historically been
// allowed by implementations, and so is also allowed here.
//
// If |in| can be normalized, returns true and sets |output| to the
// case folded, normalized value. If |in| is invalid, returns false.
// NOTE: |output| will be modified regardless of the return.
WARN_UNUSED_RESULT bool NormalizePrintableStringValue(const der::Input& in,
                                                      std::string* output) {
  in.AsString().swap(*output);
  return NormalizeDirectoryString(ENFORCE_PRINTABLE_STRING, output);
}

// Normalized a UTF8String value. See the comment for NormalizeDirectoryString
// for details.
//
// If |in| can be normalized, returns true and sets |output| to the
// case folded, normalized value. If |in| is invalid, returns false.
// NOTE: |output| will be modified regardless of the return.
WARN_UNUSED_RESULT bool NormalizeUtf8StringValue(const der::Input& in,
                                                 std::string* output) {
  in.AsString().swap(*output);
  return NormalizeDirectoryString(NO_ENFORCEMENT, output);
}

// IA5String is ISO/IEC Registrations 1 and 6 from the ISO
// "International Register of Coded Character Sets to be used
// with Escape Sequences", plus space and delete. That's just the
// polite way of saying 0x00 - 0x7F, aka ASCII (or, more formally,
// ISO/IEC 646)
//
// If |in| can be normalized, returns true and sets |output| to the case folded,
// normalized value. If |in| is invalid, returns false.
// NOTE: |output| will be modified regardless of the return.
WARN_UNUSED_RESULT bool NormalizeIA5StringValue(const der::Input& in,
                                                std::string* output) {
  in.AsString().swap(*output);
  return NormalizeDirectoryString(ENFORCE_ASCII, output);
}

// Converts BMPString value to UTF-8 and then normalizes it. See the comment for
// NormalizeDirectoryString for details.
//
// If |in| can be normalized, returns true and sets |output| to the case folded,
// normalized value. If |in| is invalid, returns false.
// NOTE: |output| will be modified regardless of the return.
WARN_UNUSED_RESULT bool NormalizeBmpStringValue(const der::Input& in,
                                                std::string* output) {
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
  if (!base::UTF16ToUTF8(in_16bit.data(), in_16bit.size(), output))
    return false;
  return NormalizeDirectoryString(NO_ENFORCEMENT, output);
}

// Converts UniversalString value to UTF-8 and then normalizes it. See the
// comment for NormalizeDirectoryString for details.
//
// If |in| can be normalized, returns true and sets |output| to the case folded,
// normalized value. If |in| is invalid, returns false.
// NOTE: |output| will be modified regardless of the return.
WARN_UNUSED_RESULT bool NormalizeUniversalStringValue(const der::Input& in,
                                                      std::string* output) {
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

    base::WriteUnicodeCharacter(codepoint, output);
  }
  return NormalizeDirectoryString(NO_ENFORCEMENT, output);
}

// Converts the string |value| to UTF-8, normalizes it, and stores in |output|.
// |tag| must one of the types for which IsNormalizableDirectoryString is true.
//
// If |value| can be normalized, returns true and sets |output| to the case
// folded, normalized value. If |value| is invalid, returns false.
// NOTE: |output| will be modified regardless of the return.
WARN_UNUSED_RESULT bool NormalizeValue(const der::Tag tag,
                                       const der::Input& value,
                                       std::string* output) {
  switch (tag) {
    case der::kPrintableString:
      return NormalizePrintableStringValue(value, output);
    case der::kUtf8String:
      return NormalizeUtf8StringValue(value, output);
    case der::kIA5String:
      return NormalizeIA5StringValue(value, output);
    case der::kUniversalString:
      return NormalizeUniversalStringValue(value, output);
    case der::kBmpString:
      return NormalizeBmpStringValue(value, output);
    default:
      NOTREACHED();
      return false;
  }
}

// Returns true if |tag| is a string type that NormalizeValue can handle.
bool IsNormalizableDirectoryString(der::Tag tag) {
  switch (tag) {
    case der::kPrintableString:
    case der::kUtf8String:
    // RFC 5280 only requires handling IA5String for comparing domainComponent
    // values, but handling it here avoids the need to special case anything.
    case der::kIA5String:
    case der::kUniversalString:
    case der::kBmpString:
      return true;
    // TeletexString isn't normalized. Section 8 of RFC 5280 briefly
    // describes the historical confusion between treating TeletexString
    // as Latin1String vs T.61, and there are even incompatibilities within
    // T.61 implementations. As this time is virtually unused, simply
    // treat it with a binary comparison, as permitted by RFC 3280/5280.
    default:
      return false;
  }
}

// Returns true if the AttributeValue (|a_tag|, |a_value|) matches (|b_tag|,
// |b_value|).
bool VerifyValueMatch(const der::Tag a_tag,
                      const der::Input& a_value,
                      const der::Tag b_tag,
                      const der::Input& b_value) {
  if (IsNormalizableDirectoryString(a_tag) &&
      IsNormalizableDirectoryString(b_tag)) {
    std::string a_normalized, b_normalized;
    if (!NormalizeValue(a_tag, a_value, &a_normalized) ||
        !NormalizeValue(b_tag, b_value, &b_normalized))
      return false;
    return a_normalized == b_normalized;
  }
  // Attributes encoded with different types may be assumed to be unequal.
  if (a_tag != b_tag)
    return false;
  // All other types use binary comparison.
  return a_value == b_value;
}

struct AttributeTypeAndValue {
  AttributeTypeAndValue(der::Input in_type,
                        der::Tag in_value_tag,
                        der::Input in_value)
      : type(in_type), value_tag(in_value_tag), value(in_value) {}
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
WARN_UNUSED_RESULT bool ReadRdn(der::Parser* parser,
                                std::vector<AttributeTypeAndValue>* out) {
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

    out->push_back(AttributeTypeAndValue(type, tag, value));
  }
  return true;
}

// Verifies that |a_parser| and |b_parser| are the same length and that every
// AttributeTypeAndValue in |a_parser| has a matching AttributeTypeAndValue in
// |b_parser|.
bool VerifyRdnMatch(der::Parser* a_parser, der::Parser* b_parser) {
  std::vector<AttributeTypeAndValue> a_type_and_values, b_type_and_values;
  if (!ReadRdn(a_parser, &a_type_and_values) ||
      !ReadRdn(b_parser, &b_type_and_values))
    return false;

  // RFC 5280 section 4.1.2.4
  // RelativeDistinguishedName ::= SET SIZE (1..MAX) OF AttributeTypeAndValue
  if (a_type_and_values.empty() || b_type_and_values.empty())
    return false;

  // RFC 5280 section 7.1:
  // Two relative distinguished names RDN1 and RDN2 match if they have the same
  // number of naming attributes and for each naming attribute in RDN1 there is
  // a matching naming attribute in RDN2.
  if (a_type_and_values.size() != b_type_and_values.size())
    return false;

  // The ordering of elements may differ due to denormalized values sorting
  // differently in the DER encoding. Since the number of elements should be
  // small, a naive linear search for each element should be fine. (Hostile
  // certificates already have ways to provoke pathological behavior.)
  for (const auto& a : a_type_and_values) {
    std::vector<AttributeTypeAndValue>::iterator b_iter =
        b_type_and_values.begin();
    for (; b_iter != b_type_and_values.end(); ++b_iter) {
      const auto& b = *b_iter;
      if (a.type == b.type &&
          VerifyValueMatch(a.value_tag, a.value, b.value_tag, b.value)) {
        break;
      }
    }
    if (b_iter == b_type_and_values.end())
      return false;
    // Remove the matched element from b_type_and_values to ensure duplicate
    // elements in a_type_and_values can't match the same element in
    // b_type_and_values multiple times.
    b_type_and_values.erase(b_iter);
  }

  // Every element in |a_type_and_values| had a matching element in
  // |b_type_and_values|.
  return true;
}

enum NameMatchType {
  EXACT_MATCH,
  SUBTREE_MATCH,
};

// Verify that |a| matches |b|. If |match_type| is EXACT_MATCH, returns true if
// they are an exact match as defined by RFC 5280 7.1. If |match_type| is
// SUBTREE_MATCH, returns true if |a| is within the subtree defined by |b| as
// defined by RFC 5280 7.1.
//
// |a| and |b| are ASN.1 RDNSequence values (not including the Sequence tag),
// defined in RFC 5280 section 4.1.2.4:
//
// Name ::= CHOICE { -- only one possibility for now --
//   rdnSequence  RDNSequence }
//
// RDNSequence ::= SEQUENCE OF RelativeDistinguishedName
//
// RelativeDistinguishedName ::=
//   SET SIZE (1..MAX) OF AttributeTypeAndValue
bool VerifyNameMatchInternal(const der::Input& a,
                             const der::Input& b,
                             NameMatchType match_type) {
  // Empty Names are allowed.  RFC 5280 section 4.1.2.4 requires "The issuer
  // field MUST contain a non-empty distinguished name (DN)", while section
  // 4.1.2.6 allows for the Subject to be empty in certain cases. The caller is
  // assumed to have verified those conditions.

  // RFC 5280 section 7.1:
  // Two distinguished names DN1 and DN2 match if they have the same number of
  // RDNs, for each RDN in DN1 there is a matching RDN in DN2, and the matching
  // RDNs appear in the same order in both DNs.

  // First just check if the inputs have the same number of RDNs:
  der::Parser a_rdn_sequence_counter(a);
  der::Parser b_rdn_sequence_counter(b);
  while (a_rdn_sequence_counter.HasMore() && b_rdn_sequence_counter.HasMore()) {
    if (!a_rdn_sequence_counter.SkipTag(der::kSet) ||
        !b_rdn_sequence_counter.SkipTag(der::kSet)) {
      return false;
    }
  }
  // If doing exact match and either of the sequences has more elements than the
  // other, not a match. If doing a subtree match, the first Name may have more
  // RDNs than the second.
  if (b_rdn_sequence_counter.HasMore())
    return false;
  if (match_type == EXACT_MATCH && a_rdn_sequence_counter.HasMore())
    return false;

  // Same number of RDNs, now check if they match.
  der::Parser a_rdn_sequence(a);
  der::Parser b_rdn_sequence(b);
  while (a_rdn_sequence.HasMore() && b_rdn_sequence.HasMore()) {
    der::Parser a_rdn, b_rdn;
    if (!a_rdn_sequence.ReadConstructed(der::kSet, &a_rdn) ||
        !b_rdn_sequence.ReadConstructed(der::kSet, &b_rdn)) {
      return false;
    }
    if (!VerifyRdnMatch(&a_rdn, &b_rdn))
      return false;
  }

  return true;
}

}  // namespace

bool VerifyNameMatch(const der::Input& a_rdn_sequence,
                     const der::Input& b_rdn_sequence) {
  return VerifyNameMatchInternal(a_rdn_sequence, b_rdn_sequence, EXACT_MATCH);
}

bool VerifyNameInSubtree(const der::Input& name_rdn_sequence,
                         const der::Input& parent_rdn_sequence) {
  return VerifyNameMatchInternal(name_rdn_sequence, parent_rdn_sequence,
                                 SUBTREE_MATCH);
}

bool NameContainsEmailAddress(const der::Input& name_rdn_sequence,
                              bool* contained_email_address) {
  der::Parser rdn_sequence_parser(name_rdn_sequence);

  while (rdn_sequence_parser.HasMore()) {
    der::Parser rdn_parser;
    if (!rdn_sequence_parser.ReadConstructed(der::kSet, &rdn_parser))
      return false;

    std::vector<AttributeTypeAndValue> type_and_values;
    if (!ReadRdn(&rdn_parser, &type_and_values))
      return false;

    for (const auto& type_and_value : type_and_values) {
      if (type_and_value.type == der::Input(kOidEmailAddress)) {
        *contained_email_address = true;
        return true;
      }
    }
  }

  *contained_email_address = false;
  return true;
}

}  // namespace net
