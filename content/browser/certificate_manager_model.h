// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CERTIFICATE_MANAGER_MODEL_H_
#define CONTENT_BROWSER_CERTIFICATE_MANAGER_MODEL_H_

#include <map>
#include <string>

#include "base/ref_counted.h"
#include "base/string16.h"
#include "net/base/cert_database.h"

// CertificateManagerModel provides the data to be displayed in the certificate
// manager dialog, and processes changes from the view.
class CertificateManagerModel {
 public:
  // Map from the subject organization name to the list of certs from that
  // organization.  If a cert does not have an organization name, the
  // subject's CertPrincipal::GetDisplayName() value is used instead.
  typedef std::map<std::string, net::CertificateList> OrgGroupingMap;

  // Enumeration of the possible columns in the certificate manager tree view.
  enum Column {
    COL_SUBJECT_NAME,
    COL_CERTIFICATE_STORE,
    COL_SERIAL_NUMBER,
    COL_EXPIRES_ON,
  };

  class Observer {
   public:
    // Called to notify the view that the certificate list has been refreshed.
    // TODO(mattm): do a more granular updating strategy?  Maybe retrieve new
    // list of certs, diff against past list, and then notify of the changes?
    virtual void CertificatesRefreshed() = 0;
  };

  explicit CertificateManagerModel(Observer* observer);
  ~CertificateManagerModel();

  // Accessor for read-only access to the underlying CertDatabase.
  const net::CertDatabase& cert_db() const { return cert_db_; }

  // Trigger a refresh of the list of certs, unlock any slots if necessary.
  // Following this call, the observer CertificatesRefreshed method will be
  // called so the view can call FilterAndBuildOrgGroupingMap as necessary to
  // refresh its tree views.
  void Refresh();

  // Fill |map| with the certificates matching |filter_type|.
  void FilterAndBuildOrgGroupingMap(net::CertType filter_type,
                                    OrgGroupingMap* map) const;

  // Get the data to be displayed in |column| for the given |cert|.
  string16 GetColumnText(const net::X509Certificate& cert, Column column) const;

  // Import certificates from PKCS #12 encoded |data|, using the given
  // |password|.  Returns a net error code on failure.
  int ImportFromPKCS12(net::CryptoModule* module, const std::string& data,
                       const string16& password);

  // Import CA certificates.
  // Tries to import all the certificates given.  The root will be trusted
  // according to |trust_bits|.  Any certificates that could not be imported
  // will be listed in |not_imported|.
  // |trust_bits| should be a bit field of TRUST_* values from CertDatabase, or
  // UNTRUSTED.
  // Returns false if there is an internal error, otherwise true is returned and
  // |not_imported| should be checked for any certificates that were not
  // imported.
  bool ImportCACerts(const net::CertificateList& certificates,
                     unsigned int trust_bits,
                     net::CertDatabase::ImportCertFailureList* not_imported);

  // Import server certificate.  The first cert should be the server cert.  Any
  // additional certs should be intermediate/CA certs and will be imported but
  // not given any trust.
  // Any certificates that could not be imported will be listed in
  // |not_imported|.
  // Returns false if there is an internal error, otherwise true is returned and
  // |not_imported| should be checked for any certificates that were not
  // imported.
  bool ImportServerCert(
      const net::CertificateList& certificates,
      net::CertDatabase::ImportCertFailureList* not_imported);

  // Set trust values for certificate.
  // |trust_bits| should be a bit field of TRUST_* values from CertDatabase, or
  // UNTRUSTED.
  // Returns true on success or false on failure.
  bool SetCertTrust(const net::X509Certificate* cert,
                    net::CertType type,
                    unsigned int trust_bits);

  // Delete the cert.  Returns true on success.  |cert| is still valid when this
  // function returns.
  bool Delete(net::X509Certificate* cert);

 private:
  // Callback used by Refresh() for when the cert slots have been unlocked.
  // This method does the actual refreshing.
  void RefreshSlotsUnlocked();

  net::CertDatabase cert_db_;
  net::CertificateList cert_list_;

  // The observer to notify when certificate list is refreshed.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(CertificateManagerModel);
};

#endif  // CONTENT_BROWSER_CERTIFICATE_MANAGER_MODEL_H_
