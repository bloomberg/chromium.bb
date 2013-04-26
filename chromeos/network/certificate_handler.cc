// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/certificate_handler.h"

#include "base/values.h"
#include "chromeos/network/onc/onc_certificate_importer.h"
#include "chromeos/network/onc/onc_utils.h"

namespace chromeos {

CertificateHandler::CertificateHandler() {
}

CertificateHandler::~CertificateHandler() {
}

bool CertificateHandler::ImportCertificates(
    const base::ListValue& certificates,
    onc::ONCSource source,
    net::CertificateList* onc_trusted_certificates) {
  VLOG(2) << "ONC file has " << certificates.GetSize() << " certificates";

  // Web trust is only granted to certificates imported by the user.
  bool allow_trust_imports = source == onc::ONC_SOURCE_USER_IMPORT;
  onc::CertificateImporter cert_importer(allow_trust_imports);
  if (cert_importer.ParseAndStoreCertificates(
          certificates, onc_trusted_certificates) !=
      onc::CertificateImporter::IMPORT_OK) {
    LOG(ERROR) << "Cannot parse some of the certificates in the ONC from "
               << onc::GetSourceAsString(source);
    return false;
  }
  return true;
}

}  // namespace chromeos
