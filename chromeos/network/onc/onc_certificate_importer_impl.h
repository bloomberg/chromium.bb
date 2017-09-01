// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_IMPL_H_
#define CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/onc/onc_certificate_importer.h"
#include "components/onc/onc_constants.h"

namespace base {
class DictionaryValue;
class ListValue;
class SequencedTaskRunner;
}

namespace net {
class NSSCertDatabase;
}

namespace chromeos {
namespace onc {

// This class handles certificate imports from ONC (both policy and user
// imports) into a certificate store. The GUID of Client certificates is stored
// together with the certificate as Nickname. In contrast, Server and CA
// certificates are identified by their PEM and not by GUID.
// TODO(pneubeck): Replace Nickname by PEM for Client
// certificates. http://crbug.com/252119
class CHROMEOS_EXPORT CertificateImporterImpl : public CertificateImporter {
 public:
  // |io_task_runner| will be used for NSSCertDatabase accesses.
  CertificateImporterImpl(
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
      net::NSSCertDatabase* target_nssdb_);
  ~CertificateImporterImpl() override;

  // CertificateImporter overrides
  void ImportCertificates(const base::ListValue& certificates,
                          ::onc::ONCSource source,
                          const DoneCallback& done_callback) override;

 private:
  void RunDoneCallback(const CertificateImporter::DoneCallback& callback,
                       bool success,
                       net::ScopedCERTCertificateList onc_trusted_certificates);

  // This is the synchronous implementation of ImportCertificates. It is
  // executed on the given |io_task_runner_|.
  static void ParseAndStoreCertificates(::onc::ONCSource source,
                                        const DoneCallback& done_callback,
                                        base::ListValue* certificates,
                                        net::NSSCertDatabase* nssdb);

  // Parses and stores |certificate| in the certificate store. Returns true if
  // the operation succeeded.
  static bool ParseAndStoreCertificate(
      ::onc::ONCSource source,
      bool allow_trust_imports,
      const base::DictionaryValue& certificate,
      net::NSSCertDatabase* nssdb,
      net::ScopedCERTCertificateList* onc_trusted_certificates);

  // Imports the Server or CA certificate |certificate|. Web trust is only
  // applied if the certificate requests the TrustBits attribute "Web" and if
  // the |allow_trust_imports| permission is granted, otherwise the attribute is
  // ignored.
  static bool ParseServerOrCaCertificate(
      ::onc::ONCSource source,
      bool allow_trust_imports,
      const std::string& cert_type,
      const std::string& guid,
      const base::DictionaryValue& certificate,
      net::NSSCertDatabase* nssdb,
      net::ScopedCERTCertificateList* onc_trusted_certificates);

  static bool ParseClientCertificate(const std::string& guid,
                                     const base::DictionaryValue& certificate,
                                     net::NSSCertDatabase* nssdb);

  // The task runner to use for NSSCertDatabase accesses.
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // The certificate database to which certificates are imported.
  net::NSSCertDatabase* target_nssdb_;

  base::WeakPtrFactory<CertificateImporterImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CertificateImporterImpl);
};

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_IMPL_H_
