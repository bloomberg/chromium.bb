// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CERTIFICATE_HANDLER_H_
#define CHROMEOS_NETWORK_CERTIFICATE_HANDLER_H_

#include "chromeos/chromeos_export.h"
#include "chromeos/network/onc/onc_constants.h"
#include "net/cert/x509_certificate.h"

namespace base {
class ListValue;
}

namespace chromeos {

class CHROMEOS_EXPORT CertificateHandler {
 public:
  CertificateHandler();
  virtual ~CertificateHandler();

  // Import the |certificates|, which must be a list of ONC Certificate objects.
  // If |onc_trusted_certificates| is not NULL, it will be filled with the list
  // of certificates that requested the TrustBit "Web". Returns true if all
  // certificates were imported successfully.
  virtual bool ImportCertificates(
      const base::ListValue& certificates,
      onc::ONCSource source,
      net::CertificateList* onc_trusted_certificates);

 private:
  DISALLOW_COPY_AND_ASSIGN(CertificateHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CERTIFICATE_HANDLER_H_
