// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_
#define CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
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
  typedef base::Callback<
      void(bool success, const net::CertificateList& onc_trusted_certificates)>
      DoneCallback;

  CertificateImporter() {}
  virtual ~CertificateImporter() {}

  // Import |certificates|, which must be a list of ONC Certificate objects.
  // Certificates are only imported with web trust for user imports. If the
  // "Remove" field of a certificate is enabled, then removes the certificate
  // from the store instead of importing.
  // When the import is completed, |done_callback| will be called with |success|
  // equal to true if all certificates were imported successfully.
  // |onc_trusted_certificates| will contain the list of certificates that
  // were imported and requested the TrustBit "Web".
  // Never calls |done_callback| after this importer is destructed.
  virtual void ImportCertificates(const base::ListValue& certificates,
                                  ::onc::ONCSource source,
                                  const DoneCallback& done_callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CertificateImporter);
};

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_
