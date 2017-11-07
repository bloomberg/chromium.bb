// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_CERTIFICATE_INFO_H_
#define CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_CERTIFICATE_INFO_H_

#include <stdint.h>

#include <vector>

#include "base/memory/ref_counted.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"

namespace chromeos {
namespace certificate_provider {

// Holds all information of a certificate that must be synchronously available
// to implement net::SSLPrivateKey.
struct CertificateInfo {
  CertificateInfo();
  CertificateInfo(const CertificateInfo& other);
  ~CertificateInfo();

  scoped_refptr<net::X509Certificate> certificate;
  // Contains the list of supported signature algorithms, using TLS 1.3's
  // SignatureScheme values. See net::SSLPrivateKey documentation for details.
  std::vector<uint16_t> supported_algorithms;
};
using CertificateInfoList = std::vector<CertificateInfo>;

}  // namespace certificate_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_CERTIFICATE_INFO_H_
