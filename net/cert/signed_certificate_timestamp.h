// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_SIGNED_CERTIFICATE_TIMESTAMP_H_
#define NET_CERT_SIGNED_CERTIFICATE_TIMESTAMP_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"

namespace net {

// Structures related to Certificate Transparency (RFC6962).
namespace ct {

// LogEntry struct in RFC 6962, Section 3.1
struct NET_EXPORT LogEntry {
  // LogEntryType enum in RFC 6962, Section 3.1
  enum Type {
    LOG_ENTRY_TYPE_X509 = 0,
    LOG_ENTRY_TYPE_PRECERT = 1
  };

  LogEntry();
  ~LogEntry();
  void Reset();

  Type type;

  // Set if type == LOG_ENTRY_TYPE_X509
  std::string leaf_certificate;

  // Set if type == LOG_ENTRY_TYPE_PRECERT
  SHA256HashValue issuer_key_hash;
  std::string tbs_certificate;
};

// Helper structure to represent Digitally Signed data, as described in
// Sections 4.7 and 7.4.1.4.1 of RFC 5246.
struct NET_EXPORT_PRIVATE DigitallySigned {
  enum HashAlgorithm {
    HASH_ALGO_NONE = 0,
    HASH_ALGO_MD5 = 1,
    HASH_ALGO_SHA1 = 2,
    HASH_ALGO_SHA224 = 3,
    HASH_ALGO_SHA256 = 4,
    HASH_ALGO_SHA384 = 5,
    HASH_ALGO_SHA512 = 6,
  };

  enum SignatureAlgorithm {
    SIG_ALGO_ANONYMOUS = 0,
    SIG_ALGO_RSA = 1,
    SIG_ALGO_DSA = 2,
    SIG_ALGO_ECDSA = 3
  };

  DigitallySigned();
  ~DigitallySigned();

  HashAlgorithm hash_algorithm;
  SignatureAlgorithm signature_algorithm;
  // 'signature' field.
  std::string signature_data;
};

// SignedCertificateTimestamp struct in RFC 6962, Section 3.2.
struct NET_EXPORT SignedCertificateTimestamp {
  // Version enum in RFC 6962, Section 3.2.
  enum Version {
    SCT_VERSION_1 = 0,
  };

  // Source of the SCT - supplementary, not defined in CT RFC.
  enum Origin {
    SCT_EMBEDDED = 0,
    SCT_FROM_TLS_HANDSHAKE = 1,
    SCT_FROM_OCSP_RESPONSE = 2,
  };

  SignedCertificateTimestamp();
  ~SignedCertificateTimestamp();

  Version version;
  std::string log_id;
  base::Time timestamp;
  std::string extensions;
  DigitallySigned signature;
  // The origin should not participate in equality checks
  // as the same SCT can be provided from multiple sources.
  Origin origin;
};

}  // namespace ct

}  // namespace net

#endif  // NET_CERT_SIGNED_CERTIFICATE_TIMESTAMP_H_
