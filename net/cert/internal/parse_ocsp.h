// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_PARSE_OCSP_H_
#define NET_CERT_INTERNAL_PARSE_OCSP_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cert/internal/parse_certificate.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/cert/ocsp_revocation_status.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"
#include "net/der/tag.h"

namespace base {
class Time;
class TimeDelta;
}

namespace net {

// OCSPCertID contains a representation of a DER-encoded RFC 6960 "CertID".
//
// CertID ::= SEQUENCE {
//    hashAlgorithm           AlgorithmIdentifier,
//    issuerNameHash          OCTET STRING, -- Hash of issuer's DN
//    issuerKeyHash           OCTET STRING, -- Hash of issuer's public key
//    serialNumber            CertificateSerialNumber
// }
struct OCSPCertID {
  OCSPCertID();
  ~OCSPCertID();

  DigestAlgorithm hash_algorithm;
  der::Input issuer_name_hash;
  der::Input issuer_key_hash;
  der::Input serial_number;
};

// OCSPCertStatus contains a representation of a DER-encoded RFC 6960
// "CertStatus". |revocation_time| and |has_reason| are only valid when
// |status| is REVOKED. |revocation_reason| is only valid when |has_reason| is
// true.
//
// CertStatus ::= CHOICE {
//      good        [0]     IMPLICIT NULL,
//      revoked     [1]     IMPLICIT RevokedInfo,
//      unknown     [2]     IMPLICIT UnknownInfo
// }
//
// RevokedInfo ::= SEQUENCE {
//      revocationTime              GeneralizedTime,
//      revocationReason    [0]     EXPLICIT CRLReason OPTIONAL
// }
//
// UnknownInfo ::= NULL
//
// CRLReason ::= ENUMERATED {
//      unspecified             (0),
//      keyCompromise           (1),
//      cACompromise            (2),
//      affiliationChanged      (3),
//      superseded              (4),
//      cessationOfOperation    (5),
//      certificateHold         (6),
//           -- value 7 is not used
//      removeFromCRL           (8),
//      privilegeWithdrawn      (9),
//      aACompromise           (10)
// }
// (from RFC 5280)
struct OCSPCertStatus {

  // Correspond to the values of CRLReason
  enum class RevocationReason {
    UNSPECIFIED = 0,
    KEY_COMPROMISE = 1,
    CA_COMPROMISE = 2,
    AFFILIATION_CHANGED = 3,
    SUPERSEDED = 4,
    CESSATION_OF_OPERATION = 5,
    CERTIFICATE_HOLD = 6,
    UNUSED = 7,
    REMOVE_FROM_CRL = 8,
    PRIVILEGE_WITHDRAWN = 9,
    AA_COMPROMISE = 10,

    LAST = AA_COMPROMISE,
  };

  OCSPRevocationStatus status;
  der::GeneralizedTime revocation_time;
  bool has_reason;
  RevocationReason revocation_reason;
};

// OCSPSingleResponse contains a representation of a DER-encoded RFC 6960
// "SingleResponse". The |cert_id_tlv| and |extensions| fields are pointers to
// the original object and are only valid as long as it is alive. They also
// aren't verified until they are parsed. |next_update| is only valid if
// |has_next_update| is true and |extensions| is only valid if |has_extensions|
// is true.
//
// SingleResponse ::= SEQUENCE {
//      certID                       CertID,
//      certStatus                   CertStatus,
//      thisUpdate                   GeneralizedTime,
//      nextUpdate         [0]       EXPLICIT GeneralizedTime OPTIONAL,
//      singleExtensions   [1]       EXPLICIT Extensions OPTIONAL
// }
struct NET_EXPORT OCSPSingleResponse {
  OCSPSingleResponse();
  ~OCSPSingleResponse();

  der::Input cert_id_tlv;
  OCSPCertStatus cert_status;
  der::GeneralizedTime this_update;
  bool has_next_update;
  der::GeneralizedTime next_update;
  bool has_extensions;
  der::Input extensions;
};

// OCSPResponseData contains a representation of a DER-encoded RFC 6960
// "ResponseData". The |responses| and |extensions| fields are pointers to the
// original object and are only valid as long as it is alive. They also aren't
// verified until they are parsed into OCSPSingleResponse and ParsedExtensions.
// |extensions| is only valid if |has_extensions| is true.
//
// ResponseData ::= SEQUENCE {
//      version              [0] EXPLICIT Version DEFAULT v1,
//      responderID              ResponderID,
//      producedAt               GeneralizedTime,
//      responses                SEQUENCE OF SingleResponse,
//      responseExtensions   [1] EXPLICIT Extensions OPTIONAL
// }
struct NET_EXPORT OCSPResponseData {
  enum class ResponderType { NAME, KEY_HASH };

  struct ResponderID {
    ResponderType type;
    der::Input name;
    der::Input key_hash;
  };

  OCSPResponseData();
  ~OCSPResponseData();

  uint8_t version;
  OCSPResponseData::ResponderID responder_id;
  der::GeneralizedTime produced_at;
  std::vector<der::Input> responses;
  bool has_extensions;
  der::Input extensions;
};

// OCSPResponse contains a representation of a DER-encoded RFC 6960
// "OCSPResponse" and the corresponding "BasicOCSPResponse". The |data| field
// is a pointer to the original object and are only valid as long is it is
// alive. The |data| field isn't verified until it is parsed into an
// OCSPResponseData. |data|, |signature_algorithm|, |signature|, and
// |has_certs| is only valid if |status| is SUCCESSFUL. |certs| is only valid
// if |has_certs| is true.
//
// OCSPResponse ::= SEQUENCE {
//      responseStatus         OCSPResponseStatus,
//      responseBytes          [0] EXPLICIT ResponseBytes OPTIONAL
// }
//
// ResponseBytes ::=       SEQUENCE {
//      responseType   OBJECT IDENTIFIER,
//      response       OCTET STRING
// }
//
// BasicOCSPResponse       ::= SEQUENCE {
//      tbsResponseData      ResponseData,
//      signatureAlgorithm   AlgorithmIdentifier,
//      signature            BIT STRING,
//      certs            [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL
// }
//
// OCSPResponseStatus ::= ENUMERATED {
//     successful            (0),  -- Response has valid confirmations
//     malformedRequest      (1),  -- Illegal confirmation request
//     internalError         (2),  -- Internal error in issuer
//     tryLater              (3),  -- Try again later
//                                 -- (4) is not used
//     sigRequired           (5),  -- Must sign the request
//     unauthorized          (6)   -- Request unauthorized
// }
struct NET_EXPORT OCSPResponse {
  // Correspond to the values of OCSPResponseStatus
  enum class ResponseStatus {
    SUCCESSFUL = 0,
    MALFORMED_REQUEST = 1,
    INTERNAL_ERROR = 2,
    TRY_LATER = 3,
    UNUSED = 4,
    SIG_REQUIRED = 5,
    UNAUTHORIZED = 6,

    LAST = UNAUTHORIZED,
  };

  OCSPResponse();
  ~OCSPResponse();

  ResponseStatus status;
  der::Input data;
  std::unique_ptr<SignatureAlgorithm> signature_algorithm;
  der::BitString signature;
  bool has_certs;
  std::vector<der::Input> certs;
};

// From RFC 6960:
//
// id-pkix-ocsp           OBJECT IDENTIFIER ::= { id-ad-ocsp }
// id-pkix-ocsp-basic     OBJECT IDENTIFIER ::= { id-pkix-ocsp 1 }
//
// In dotted notation: 1.3.6.1.5.5.7.48.1.1
NET_EXPORT der::Input BasicOCSPResponseOid();

// Parses a DER-encoded OCSP "CertID" as specified by RFC 6960. Returns true on
// success and sets the results in |out|.
//
// On failure |out| has an undefined state. Some of its fields may have been
// updated during parsing, whereas others may not have been changed.
NET_EXPORT_PRIVATE bool ParseOCSPCertID(const der::Input& raw_tlv,
                                        OCSPCertID* out);

// Parses a DER-encoded OCSP "SingleResponse" as specified by RFC 6960. Returns
// true on success and sets the results in |out|. The resulting |out|
// references data from |raw_tlv| and is only valid for the lifetime of
// |raw_tlv|.
//
// On failure |out| has an undefined state. Some of its fields may have been
// updated during parsing, whereas others may not have been changed.
NET_EXPORT_PRIVATE bool ParseOCSPSingleResponse(const der::Input& raw_tlv,
                                                OCSPSingleResponse* out);

// Parses a DER-encoded OCSP "ResponseData" as specified by RFC 6960. Returns
// true on success and sets the results in |out|. The resulting |out|
// references data from |raw_tlv| and is only valid for the lifetime of
// |raw_tlv|.
//
// On failure |out| has an undefined state. Some of its fields may have been
// updated during parsing, whereas others may not have been changed.
NET_EXPORT_PRIVATE bool ParseOCSPResponseData(const der::Input& raw_tlv,
                                              OCSPResponseData* out);

// Parses a DER-encoded "OCSPResponse" as specified by RFC 6960. Returns true
// on success and sets the results in |out|. The resulting |out|
// references data from |raw_tlv| and is only valid for the lifetime of
// |raw_tlv|.
//
// On failure |out| has an undefined state. Some of its fields may have been
// updated during parsing, whereas others may not have been changed.
NET_EXPORT_PRIVATE bool ParseOCSPResponse(const der::Input& raw_tlv,
                                          OCSPResponse* out);

// Checks the certificate status of |cert_tbs_certificate_tlv| based on the
// OCSPResponseData |response_data| and issuer |issuer_tbs_certificate_tlv| and
// sets the results in |out|. In the case that there are multiple responses for
// a given certificate, as a result of caching or performance (RFC 6960,
// 4.2.2.3), the strictest response is returned (REVOKED > UNKNOWN > GOOD).
//
// On failure |out| has an undefined state. Some of its fields may have been
// updated during parsing, whereas others may not have been changed.
NET_EXPORT_PRIVATE bool GetOCSPCertStatus(
    const OCSPResponseData& response_data,
    const der::Input& issuer_tbs_certificate_tlv,
    const der::Input& cert_tbs_certificate_tlv,
    OCSPCertStatus* out);

// Returns true if |response|, a valid OCSP response with a thisUpdate field and
// potentially a nextUpdate field, is valid at |verify_time| and not older than
// |max_age|. Expressed differently, returns true if |response.thisUpdate| <=
// |verify_time| < response.nextUpdate, and |response.thisUpdate| >=
// |verify_time| - |max_age|.
NET_EXPORT_PRIVATE bool CheckOCSPDateValid(const OCSPSingleResponse& response,
                                           const base::Time& verify_time,
                                           const base::TimeDelta& max_age);

}  // namespace net

#endif  // NET_CERT_INTERNAL_PARSE_OCSP_H_
