// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_manager_model.h"

#include <utility>

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/ui/crypto_module_password_dialog_nss.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "crypto/nss_util.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"

// TODO(wychen): ChromeOS headers should only be included when building
//               ChromeOS, and the following headers should be guarded by
//               #if defined(OS_CHROMEOS). However, the types are actually
//               used, and it takes another CL to clean them up.
//               Reference: crbug.com/720159
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"

using content::BrowserThread;

// CertificateManagerModel is created on the UI thread. It needs a
// NSSCertDatabase handle (and on ChromeOS it needs to get the TPM status) which
// needs to be done on the IO thread.
//
// The initialization flow is roughly:
//
//               UI thread                              IO Thread
//
//   CertificateManagerModel::Create
//                  \--------------------------------------v
//                                CertificateManagerModel::GetCertDBOnIOThread
//                                                         |
//                                     GetNSSCertDatabaseForResourceContext
//                                                         |
//                               CertificateManagerModel::DidGetCertDBOnIOThread
//                                                         |
//                                       crypto::IsTPMTokenEnabledForNSS
//                  v--------------------------------------/
// CertificateManagerModel::DidGetCertDBOnUIThread
//                  |
//     new CertificateManagerModel
//                  |
//               callback

namespace {

std::string GetCertificateOrg(net::X509Certificate* cert) {
    std::string org;
    if (!cert->subject().organization_names.empty())
      org = cert->subject().organization_names[0];
    if (org.empty())
      org = cert->subject().GetDisplayName();

    return org;
}

}  // namespace

// static
void CertificateManagerModel::Create(
    content::BrowserContext* browser_context,
    CertificateManagerModel::Observer* observer,
    const CreationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<chromeos::CertificateProvider> extension_certificate_provider;
#if defined(OS_CHROMEOS)
  chromeos::CertificateProviderService* service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context);
  extension_certificate_provider = service->CreateCertificateProvider();
#endif

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&CertificateManagerModel::GetCertDBOnIOThread,
                     browser_context->GetResourceContext(), observer,
                     base::Passed(&extension_certificate_provider), callback));
}

CertificateManagerModel::CertificateManagerModel(
    net::NSSCertDatabase* nss_cert_database,
    bool is_user_db_available,
    bool is_tpm_available,
    Observer* observer,
    std::unique_ptr<chromeos::CertificateProvider>
        extension_certificate_provider)
    : cert_db_(nss_cert_database),
      is_user_db_available_(is_user_db_available),
      is_tpm_available_(is_tpm_available),
      observer_(observer),
      extension_certificate_provider_(std::move(
                                          extension_certificate_provider)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CertificateManagerModel::~CertificateManagerModel() {
}

void CertificateManagerModel::Refresh() {
  DVLOG(1) << "refresh started";
  std::vector<crypto::ScopedPK11Slot> modules;
  cert_db_->ListModules(&modules, false);
  DVLOG(1) << "refresh waiting for unlocking...";
  chrome::UnlockSlotsIfNecessary(
      std::move(modules), chrome::kCryptoModulePasswordListCerts,
      net::HostPortPair(),  // unused.
      NULL,                 // TODO(mattm): supply parent window.
      base::Bind(&CertificateManagerModel::RefreshSlotsUnlocked,
                 base::Unretained(this)));

#if defined(OS_CHROMEOS)
  extension_certificate_provider_->GetCertificates(base::Bind(
      &CertificateManagerModel::RefreshExtensionCertificates,
      weak_ptr_factory_.GetWeakPtr()));
#endif
}

void CertificateManagerModel::RefreshSlotsUnlocked() {
  DVLOG(1) << "refresh listing certs...";
  // TODO(tbarzic): Use async |ListCerts|.
  cert_db_->ListCertsSync(&cert_list_);
  observer_->CertificatesRefreshed();
  DVLOG(1) << "refresh finished for platform provided certificates";
}

void CertificateManagerModel::RefreshExtensionCertificates(
    net::ClientCertIdentityList new_cert_identities) {
  extension_cert_list_.clear();
  extension_cert_list_.reserve(new_cert_identities.size());
  for (const auto& identity : new_cert_identities)
    extension_cert_list_.push_back(identity->certificate());
  observer_->CertificatesRefreshed();
  DVLOG(1) << "refresh finished for extension provided certificates";
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

    std::string org = GetCertificateOrg(cert);
    (*map)[org].push_back(cert);
  }

  // Display extension provided certificates under the "Your Certificates" tab.
  if (filter_type == net::USER_CERT) {
    for (const auto& cert : extension_cert_list_) {
      std::string org = GetCertificateOrg(cert.get());
      (*map)[org].push_back(cert);
    }
  }
}

base::string16 CertificateManagerModel::GetColumnText(
    const net::X509Certificate& cert,
    Column column) const {
  base::string16 rv;
  switch (column) {
    case COL_SUBJECT_NAME:
      rv = base::UTF8ToUTF16(
          x509_certificate_model::GetCertNameOrNickname(cert.os_cert_handle()));

      // Mark extension provided certificates.
      if (base::ContainsValue(extension_cert_list_, &cert)) {
        rv = l10n_util::GetStringFUTF16(
            IDS_CERT_MANAGER_EXTENSION_PROVIDED_FORMAT,
            rv);
      } else if (IsHardwareBacked(&cert)) {
        // TODO(xiyuan): Put this into a column when we have js tree-table.
        rv = l10n_util::GetStringFUTF16(
            IDS_CERT_MANAGER_HARDWARE_BACKED_KEY_FORMAT,
            rv,
            l10n_util::GetStringUTF16(IDS_CERT_MANAGER_HARDWARE_BACKED));
      }
      break;
    case COL_CERTIFICATE_STORE:
      rv = base::UTF8ToUTF16(
          x509_certificate_model::GetTokenName(cert.os_cert_handle()));
      break;
    case COL_SERIAL_NUMBER:
      rv = base::ASCIIToUTF16(x509_certificate_model::GetSerialNumberHexified(
          cert.os_cert_handle(), std::string()));
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

int CertificateManagerModel::ImportFromPKCS12(PK11SlotInfo* slot_info,
                                              const std::string& data,
                                              const base::string16& password,
                                              bool is_extractable) {
  int result = cert_db_->ImportFromPKCS12(slot_info, data,
                                          password, is_extractable, NULL);
  if (result == net::OK)
    Refresh();
  return result;
}

int CertificateManagerModel::ImportUserCert(const std::string& data) {
  int result = cert_db_->ImportUserCert(data);
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
  return cert_db_->IsHardwareBacked(cert);
}

// static
void CertificateManagerModel::DidGetCertDBOnUIThread(
    net::NSSCertDatabase* cert_db,
    bool is_user_db_available,
    bool is_tpm_available,
    CertificateManagerModel::Observer* observer,
    std::unique_ptr<chromeos::CertificateProvider>
        extension_certificate_provider,
    const CreationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<CertificateManagerModel> model(new CertificateManagerModel(
      cert_db, is_user_db_available, is_tpm_available, observer,
      std::move(extension_certificate_provider)));
  callback.Run(std::move(model));
}

// static
void CertificateManagerModel::DidGetCertDBOnIOThread(
    CertificateManagerModel::Observer* observer,
    std::unique_ptr<chromeos::CertificateProvider>
        extension_certificate_provider,
    const CreationCallback& callback,
    net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  bool is_user_db_available = !!cert_db->GetPublicSlot();
  bool is_tpm_available = false;
#if defined(OS_CHROMEOS)
  is_tpm_available = crypto::IsTPMTokenEnabledForNSS();
#endif
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&CertificateManagerModel::DidGetCertDBOnUIThread, cert_db,
                     is_user_db_available, is_tpm_available, observer,
                     base::Passed(&extension_certificate_provider), callback));
}

// static
void CertificateManagerModel::GetCertDBOnIOThread(
    content::ResourceContext* context,
    CertificateManagerModel::Observer* observer,
    std::unique_ptr<chromeos::CertificateProvider>
        extension_certificate_provider,
    const CreationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto did_get_cert_db_callback = base::Bind(
      &CertificateManagerModel::DidGetCertDBOnIOThread, observer,
      base::Passed(&extension_certificate_provider), callback);

  net::NSSCertDatabase* cert_db = GetNSSCertDatabaseForResourceContext(
      context, did_get_cert_db_callback);

  // The callback is run here instead of the actual function call because of
  // extension_certificate_provider ownership semantics, ie. ownership can only
  // be released once. The callback will only be run once (either inside the
  // function above or here).
  if (cert_db)
    did_get_cert_db_callback.Run(cert_db);
}
