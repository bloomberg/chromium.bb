// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_X509_OPENSSL_UTIL_H_
#define NET_BASE_X509_OPENSSL_UTIL_H_
#pragma once

#include <openssl/asn1.h>
#include <openssl/x509v3.h>

#include <string>
#include <vector>

namespace base {
class Time;
}  // namespace base

namespace net {

// A collection of helper functions to fetch data from OpenSSL X509 certificates
// into more convenient std / base datatypes.
namespace x509_openssl_util {

bool ParsePrincipalKeyAndValueByIndex(X509_NAME* name,
                                      int index,
                                      std::string* key,
                                      std::string* value);

bool ParsePrincipalValueByIndex(X509_NAME* name, int index, std::string* value);

bool ParsePrincipalValueByNID(X509_NAME* name, int nid, std::string* value);

bool ParseDate(ASN1_TIME* x509_time, base::Time* time);

// Verifies that |hostname| matches one of the names in |cert_names|, based on
// TLS name matching rules, specifically following http://tools.ietf.org/html/draft-saintandre-tls-server-id-check-09#section-4.4.3
// The members of |cert_names| must have been extracted from the Subject CN or
// SAN fields of a certificate.
bool VerifyHostname(const std::string& hostname,
                    const std::vector<std::string>& cert_names);

} // namespace x509_openssl_util

} // namespace net

#endif  // NET_BASE_X509_OPENSSL_UTIL_H_
