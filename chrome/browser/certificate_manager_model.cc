// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_manager_model.h"

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "net/base/crypto_module.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"

#if defined(OS_CHROMEOS)
#include <cert.h>

#include "crypto/nss_util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#endif

CertificateManagerModel::CertificateManagerModel(Observer* observer)
    : cert_db_(net::NSSCertDatabase::GetInstance()),
      observer_(observer) {
}

CertificateManagerModel::~CertificateManagerModel() {
}

void CertificateManagerModel::Refresh() {
  VLOG(1) << "refresh started";
  net::CryptoModuleList modules;
  cert_db_->ListModules(&modules, false);
  VLOG(1) << "refresh waiting for unlocking...";
  chrome::UnlockSlotsIfNecessary(
      modules,
      chrome::kCryptoModulePasswordListCerts,
      "",  // unused.
      base::Bind(&CertificateManagerModel::RefreshSlotsUnlocked,
                 base::Unretained(this)));
}

void CertificateManagerModel::RefreshSlotsUnlocked() {
  VLOG(1) << "refresh listing certs...";
  cert_db_->ListCerts(&cert_list_);
  observer_->CertificatesRefreshed();
  VLOG(1) << "refresh finished";
}

void CertificateManagerModel::FilterAndBuildOrgGroupingMap(
    net::CertType filter_type,
    CertificateManagerModel::OrgGroupingMap* map) const {
  for (net::CertificateList::const_iterator i = cert_list_.begin();
       i != cert_list_.end(); ++i) {
    net::X509Certificate* cert = i->get();
    net::CertType type =
        x509_certificate_model::GetType(cert->os_cert_handle());
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
      rv = UTF8ToUTF16(
          x509_certificate_model::GetCertNameOrNickname(cert.os_cert_handle()));

#if defined(OS_CHROMEOS)
      // TODO(xiyuan): Put this into a column when we have js tree-table.
      if (IsHardwareBacked(&cert)) {
        rv = l10n_util::GetStringFUTF16(
            IDS_CERT_MANAGER_HARDWARE_BACKED_KEY_FORMAT,
            rv,
            l10n_util::GetStringUTF16(IDS_CERT_MANAGER_HARDWARE_BACKED));
      }
#endif
      break;
    case COL_CERTIFICATE_STORE:
      rv = UTF8ToUTF16(
          x509_certificate_model::GetTokenName(cert.os_cert_handle()));
      break;
    case COL_SERIAL_NUMBER:
      rv = ASCIIToUTF16(
          x509_certificate_model::GetSerialNumberHexified(
              cert.os_cert_handle(), ""));
      break;
    case COL_EXPIRES_ON:
      if (!cert.valid_expiry().is_null())
        rv = base::TimeFormatShortDateNumeric(cert.valid_expiry());
      break;
    default:
      NOTREACHED();
  }
  return rv;
}

int CertificateManagerModel::ImportFromPKCS12(net::CryptoModule* module,
                                              const std::string& data,
                                              const string16& password,
                                              bool is_extractable) {
  int result = cert_db_->ImportFromPKCS12(module, data, password,
                                          is_extractable, NULL);
  if (result == net::OK)
    Refresh();
  return result;
}

bool CertificateManagerModel::ImportCACerts(
    const net::CertificateList& certificates,
    net::NSSCertDatabase::TrustBits trust_bits,
    net::NSSCertDatabase::ImportCertFailureList* not_imported) {
  bool result = cert_db_->ImportCACerts(certificates, trust_bits, not_imported);
  if (result && not_imported->size() != certificates.size())
    Refresh();
  return result;
}

bool CertificateManagerModel::ImportServerCert(
    const net::CertificateList& certificates,
    net::NSSCertDatabase::TrustBits trust_bits,
    net::NSSCertDatabase::ImportCertFailureList* not_imported) {
  bool result = cert_db_->ImportServerCert(certificates, trust_bits,
                                           not_imported);
  if (result && not_imported->size() != certificates.size())
    Refresh();
  return result;
}

bool CertificateManagerModel::SetCertTrust(
    const net::X509Certificate* cert,
    net::CertType type,
    net::NSSCertDatabase::TrustBits trust_bits) {
  return cert_db_->SetCertTrust(cert, type, trust_bits);
}

bool CertificateManagerModel::Delete(net::X509Certificate* cert) {
  bool result = cert_db_->DeleteCertAndKey(cert);
  if (result)
    Refresh();
  return result;
}

bool CertificateManagerModel::IsHardwareBacked(
    const net::X509Certificate* cert) const {
#if defined(OS_CHROMEOS)
  return crypto::IsTPMTokenReady() &&
         cert->os_cert_handle()->slot ==
             cert_db_->GetPrivateModule()->os_module_handle();
#else
  return false;
#endif
}
