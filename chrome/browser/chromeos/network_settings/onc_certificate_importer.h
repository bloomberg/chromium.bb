// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NETWORK_SETTINGS_ONC_CERTIFICATE_IMPORTER_H_
#define CHROME_BROWSER_CHROMEOS_NETWORK_SETTINGS_ONC_CERTIFICATE_IMPORTER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/network_ui_data.h"

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
// imports) into the certificate store. In particular, the GUID of certificates
// is stored together with the certificate as Nickname.
class CertificateImporter {
 public:
  // Certificates pushed from a policy source with Web trust are only imported
  // with ParseCertificate() if the |allow_web_trust_from_policy| permission is
  // granted.
  CertificateImporter(NetworkUIData::ONCSource onc_source,
                      bool allow_web_trust_from_policy);

  // Parses and stores the certificates in |onc_certificates| into the
  // certificate store. If the "Remove" field of a certificate is enabled, then
  // removes the certificate from the store instead of importing. Returns false
  // and sets |error| to a user readable message if an error occured. In that
  // case, some of the certificates may already be stored/removed. Otherwise, if
  // no error occured, returns true and doesn't modify |error|.
  bool ParseAndStoreCertificates(const base::ListValue& onc_certificates,
                                 std::string* error);

  // Parses and stores/removes |certificate| in/from the certificate
  // store. Returns false if an error occured. Returns true otherwise.
  bool ParseAndStoreCertificate(const base::DictionaryValue& certificate);

  // Lists the certificates that have the string |label| as their certificate
  // nickname (exact match).
  static void ListCertsWithNickname(const std::string& label,
                                    net::CertificateList* result);

 protected:
  // Deletes any certificate that has the string |label| as its nickname (exact
  // match).
  static bool DeleteCertAndKeyByNickname(const std::string& label);

 private:
  bool ParseServerOrCaCertificate(
    const std::string& cert_type,
    const std::string& guid,
    const base::DictionaryValue& certificate);
  bool ParseClientCertificate(
    const std::string& guid,
    const base::DictionaryValue& certificate);

  // Where the ONC blob comes from.
  NetworkUIData::ONCSource onc_source_;

  // Whether certificates with Web trust should be stored when pushed from a
  // policy source.
  bool allow_web_trust_from_policy_;

  std::string error_;

  DISALLOW_COPY_AND_ASSIGN(CertificateImporter);
};

}  // chromeos
}  // onc

#endif  // CHROME_BROWSER_CHROMEOS_NETWORK_SETTINGS_ONC_CERTIFICATE_IMPORTER_H_
