// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/parse_certificate.h"

#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"

namespace net {

namespace {

// Returns true if |input| is a SEQUENCE and nothing else.
WARN_UNUSED_RESULT bool IsSequenceTLV(const der::Input& input) {
  der::Parser parser(input);
  der::Parser unused_sequence_parser;
  if (!parser.ReadSequence(&unused_sequence_parser))
    return false;
  // Should by a single SEQUENCE by definition of the function.
  return !parser.HasMore();
}

// Reads a SEQUENCE from |parser| and writes the full tag-length-value into
// |out|. On failure |parser| may or may not have been advanced.
WARN_UNUSED_RESULT bool ReadSequenceTLV(der::Parser* parser, der::Input* out) {
  return parser->ReadRawTLV(out) && IsSequenceTLV(*out);
}

}  // namespace

bool ParseCertificate(const der::Input& certificate_tlv,
                      ParsedCertificate* out) {
  der::Parser parser(certificate_tlv);

  //   Certificate  ::=  SEQUENCE  {
  der::Parser certificate_parser;
  if (!parser.ReadSequence(&certificate_parser))
    return false;

  //        tbsCertificate       TBSCertificate,
  if (!ReadSequenceTLV(&certificate_parser, &out->tbs_certificate_tlv))
    return false;

  //        signatureAlgorithm   AlgorithmIdentifier,
  if (!ReadSequenceTLV(&certificate_parser, &out->signature_algorithm_tlv))
    return false;

  //        signatureValue       BIT STRING  }
  if (!certificate_parser.ReadBitString(&out->signature_value))
    return false;

  // There isn't an extension point at the end of Certificate.
  if (certificate_parser.HasMore())
    return false;

  // By definition the input was a single Certificate, so there shouldn't be
  // unconsumed data.
  if (parser.HasMore())
    return false;

  return true;
}

}  // namespace net
