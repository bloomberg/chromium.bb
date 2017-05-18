// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CERTIFICATE_HELPER_H_
#define CHROMEOS_NETWORK_CERTIFICATE_HELPER_H_

#include <cert.h>
#include <string>

#include "chromeos/chromeos_export.h"
#include "net/cert/cert_type.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {
namespace certificate {

// Selected functions from chrome/common/net/x509_certificate_model.cc

// Decodes the certificate type from |cert|.
CHROMEOS_EXPORT net::CertType GetCertType(
    net::X509Certificate::OSCertHandle cert_handle);

// Extracts the token name from |cert|->slot if it exists or returns an empty
// string.
CHROMEOS_EXPORT std::string GetCertTokenName(
    net::X509Certificate::OSCertHandle cert_handle);

// Returns the common name for |cert_handle|->issuer or |alternative_text| if
// the common name is missing or empty.
CHROMEOS_EXPORT std::string GetIssuerCommonName(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

// Returns the common name for |cert_handle|->subject converted to unicode,
// or |cert_handle|->nickname if the subject name is unavailable or empty.
// NOTE: Unlike x509_certificate_model::GetCertNameOrNickname in src/chrome,
// the raw unformated name or nickname will not be included in the returned
// value (see GetCertAsciiNameOrNickname instead).
CHROMEOS_EXPORT std::string GetCertNameOrNickname(
    net::X509Certificate::OSCertHandle cert_handle);

// Returns the unformated ASCII common name for |cert_handle|->subject.
CHROMEOS_EXPORT std::string GetCertAsciiNameOrNickname(
    net::X509Certificate::OSCertHandle cert_handle);

}  // namespace certificate
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CERTIFICATE_HELPER_H_
