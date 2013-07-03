// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_
#define CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/onc/onc_constants.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;
}

namespace chromeos {
namespace onc {

// This class handles certificate imports from ONC (both policy and user
// imports) into the certificate store. The GUID of Client certificates is
// stored together with the certificate as Nickname. In contrast, Server and CA
// certificates are identified by their PEM and not by GUID.
// TODO(pneubeck): Replace Nickname by PEM for Client
// certificates. http://crbug.com/252119
class CHROMEOS_EXPORT CertificateImporter {
 public:
  typedef std::map<std::string, scoped_refptr<net::X509Certificate> >
      CertsByGUID;
  enum ParseResult {
    IMPORT_OK,
    IMPORT_INCOMPLETE,
    IMPORT_FAILED,
  };

  // During import with ParseCertificate(), Web trust is only applied to Server
  // and Authority certificates with the TrustBits attribute "Web" if the
  // |allow_trust_imports| permission is granted, otherwise the attribute is
  // ignored.
  explicit CertificateImporter(bool allow_trust_imports);

  // Parses and stores the certificates in |onc_certificates| into the
  // certificate store. If the "Remove" field of a certificate is enabled, then
  // removes the certificate from the store instead of importing. Returns the
  // result of the parse operation. In case of IMPORT_INCOMPLETE, some of the
  // certificates may be stored/removed successfully while others had errors.
  // If no error occurred, returns IMPORT_OK. If |onc_trusted_certificates| is
  // not NULL, it will be filled with the list of certificates that requested
  // the Web trust flag. If |imported_server_and_ca_certs| is not null, it will
  // be filled with the (GUID, Certificate) pairs of all successfully imported
  // Server and CA certificates.
  ParseResult ParseAndStoreCertificates(
      const base::ListValue& onc_certificates,
      net::CertificateList* onc_trusted_certificates,
      CertsByGUID* imported_server_and_ca_certs);

  // Lists the certificates that have the string |label| as their certificate
  // nickname (exact match).
  static void ListCertsWithNickname(const std::string& label,
                                    net::CertificateList* result);

 protected:
  // Deletes any certificate that has the string |label| as its nickname (exact
  // match).
  static bool DeleteCertAndKeyByNickname(const std::string& label);

 private:
  // Parses and stores/removes |certificate| in/from the certificate
  // store. Returns true if the operation succeeded.
  bool ParseAndStoreCertificate(
      const base::DictionaryValue& certificate,
      net::CertificateList* onc_trusted_certificates,
      CertsByGUID* imported_server_and_ca_certs);

  bool ParseServerOrCaCertificate(
      const std::string& cert_type,
      const std::string& guid,
      const base::DictionaryValue& certificate,
      net::CertificateList* onc_trusted_certificates,
      CertsByGUID* imported_server_and_ca_certs);

  bool ParseClientCertificate(const std::string& guid,
                              const base::DictionaryValue& certificate);

  // Whether certificates with TrustBits attribute "Web" should be stored with
  // web trust.
  bool allow_trust_imports_;

  DISALLOW_COPY_AND_ASSIGN(CertificateImporter);
};

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_
