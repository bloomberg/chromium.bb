// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERTIFICATE_MANAGER_MODEL_H_
#define CHROME_BROWSER_CERTIFICATE_MANAGER_MODEL_H_

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
    COL_EMAIL_ADDRESS,
  };

  CertificateManagerModel();
  ~CertificateManagerModel();

  // Refresh the list of certs.  Following this call, the view should call
  // FilterAndBuildOrgGroupingMap as necessary to refresh its tree views.
  void Refresh();

  // Fill |map| with the certificates matching |filter_type|.
  void FilterAndBuildOrgGroupingMap(net::CertType filter_type,
                                    OrgGroupingMap* map) const;

  // Get the data to be displayed in |column| for the given |cert|.
  string16 GetColumnText(const net::X509Certificate& cert, Column column) const;

 private:
  net::CertDatabase cert_db_;
  net::CertificateList cert_list_;

  DISALLOW_COPY_AND_ASSIGN(CertificateManagerModel);
};

#endif  // CHROME_BROWSER_CERTIFICATE_MANAGER_MODEL_H_
