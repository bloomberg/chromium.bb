// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_
#define CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"
#include "components/onc/onc_constants.h"
#include "net/cert/x509_certificate.h"

namespace base {
class ListValue;
}

namespace chromeos {
namespace onc {

class CHROMEOS_EXPORT CertificateImporter {
 public:
  CertificateImporter() {}
  virtual ~CertificateImporter() {}

  // Import the |certificates|, which must be a list of ONC Certificate objects.
  // Certificates are only imported with web trust for user imports. If
  // |onc_trusted_certificates| is not NULL, it will be filled with the list
  // of certificates that requested the TrustBit "Web". If the "Remove" field of
  // a certificate is enabled, then removes the certificate from the store
  // instead of importing. Returns true if all certificates were imported
  // successfully.
  virtual bool ImportCertificates(
      const base::ListValue& certificates,
      ::onc::ONCSource source,
      net::CertificateList* onc_trusted_certificates) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CertificateImporter);
};

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_
