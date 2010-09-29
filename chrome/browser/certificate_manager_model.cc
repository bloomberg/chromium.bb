// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_manager_model.h"

#include <cert.h>

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/third_party/mozilla_security_manager/nsNSSCertHelper.h"
#include "chrome/third_party/mozilla_security_manager/nsNSSCertificate.h"
#include "net/base/x509_certificate.h"

// TODO(mattm): Try to make this use only X509Certificate stuff rather than NSS
// functions in some places. (Not very important at this time since this is only
// used w/NSS anyway.)

// PSM = Mozilla's Personal Security Manager.
namespace psm = mozilla_security_manager;

namespace {

// Convert a char* return value from NSS into a std::string and free the NSS
// memory.  If the arg is NULL, an empty string will be returned instead.
std::string Stringize(char* nss_text) {
  std::string s;
  if (nss_text) {
    s = nss_text;
    PORT_Free(nss_text);
  }
  return s;
}

std::string GetCertNameOrNickname(CERTCertificate* os_cert) {
  std::string name = psm::ProcessIDN(
      Stringize(CERT_GetCommonName(&os_cert->subject)));
  if (name.empty() && os_cert->nickname) {
    name = os_cert->nickname;
    // Hack copied from mozilla: Cut off text before first :, which seems to
    // just be the token name.
    size_t colon_pos = name.find(':');
    if (colon_pos != std::string::npos)
      name = name.substr(colon_pos + 1);
  }
  return name;
}

}  // namespace

CertificateManagerModel::CertificateManagerModel() {
}

CertificateManagerModel::~CertificateManagerModel() {
}

void CertificateManagerModel::Refresh() {
  cert_db_.ListCerts(&cert_list_);
}

void CertificateManagerModel::FilterAndBuildOrgGroupingMap(
    net::CertType filter_type,
    CertificateManagerModel::OrgGroupingMap* map) const {
  for (net::CertificateList::const_iterator i = cert_list_.begin();
       i != cert_list_.end(); ++i) {
    net::X509Certificate* cert = i->get();
    net::CertType type = psm::GetCertType(cert->os_cert_handle());
    if (type != filter_type)
      continue;

    std::string org;
    if (!cert->subject().organization_names.empty())
      org = cert->subject().organization_names[0];
    if (org.empty())
      org = cert->subject().GetDisplayName();

    (*map)[org].push_back(cert);
  }
}

string16 CertificateManagerModel::GetColumnText(
    const net::X509Certificate& cert,
    Column column) const {
  string16 rv;
  switch (column) {
    case COL_SUBJECT_NAME:
      rv = UTF8ToUTF16(GetCertNameOrNickname(cert.os_cert_handle()));
      break;
    case COL_CERTIFICATE_STORE:
      rv = UTF8ToUTF16(psm::GetCertTokenName(cert.os_cert_handle()));
      break;
    case COL_SERIAL_NUMBER:
      rv = ASCIIToUTF16(Stringize(CERT_Hexify(
          &cert.os_cert_handle()->serialNumber, PR_TRUE)));
      break;
    case COL_EXPIRES_ON:
      if (!cert.valid_expiry().is_null()) {
        rv = WideToUTF16Hack(
            base::TimeFormatShortDateNumeric(cert.valid_expiry()));
      }
      break;
    case COL_EMAIL_ADDRESS:
      if (cert.os_cert_handle()->emailAddr)
        rv = UTF8ToUTF16(cert.os_cert_handle()->emailAddr);
      break;
    default:
      NOTREACHED();
  }
  return rv;
}
