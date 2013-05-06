// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cert_library.h"

#include <algorithm>

#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/i18n/string_compare.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"  // g_browser_process
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/login_state.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util.h"
#include "grit/generated_resources.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "third_party/icu/public/i18n/unicode/coll.h"  // icu::Collator
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"

namespace chromeos {

namespace {

// Root CA certificates that are built into Chrome use this token name.
const char kRootCertificateTokenName[] = "Builtin Object Token";

string16 GetDisplayString(net::X509Certificate* cert, bool hardware_backed) {
  std::string org;
  if (!cert->subject().organization_names.empty())
    org = cert->subject().organization_names[0];
  if (org.empty())
    org = cert->subject().GetDisplayName();
  string16 issued_by = UTF8ToUTF16(
      x509_certificate_model::GetIssuerCommonName(cert->os_cert_handle(),
                                                  org));  // alternative text
  string16 issued_to = UTF8ToUTF16(
      x509_certificate_model::GetCertNameOrNickname(cert->os_cert_handle()));

  if (hardware_backed) {
    return l10n_util::GetStringFUTF16(
        IDS_CERT_MANAGER_HARDWARE_BACKED_KEY_FORMAT_LONG,
        issued_by,
        issued_to,
        l10n_util::GetStringUTF16(IDS_CERT_MANAGER_HARDWARE_BACKED));
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_CERT_MANAGER_KEY_FORMAT_LONG,
        issued_by,
        issued_to);
  }
}

}  // namespace

class CertNameComparator {
 public:
  explicit CertNameComparator(icu::Collator* collator)
      : collator_(collator) {
  }

  bool operator()(const scoped_refptr<net::X509Certificate>& lhs,
                  const scoped_refptr<net::X509Certificate>& rhs) const {
    string16 lhs_name = GetDisplayString(lhs.get(), false);
    string16 rhs_name = GetDisplayString(rhs.get(), false);
    if (collator_ == NULL)
      return lhs_name < rhs_name;
    return base::i18n::CompareString16WithCollator(
        collator_, lhs_name, rhs_name) == UCOL_LESS;
  }

 private:
  icu::Collator* collator_;
};

static CertLibrary* g_cert_library = NULL;

// static
void CertLibrary::Initialize() {
  CHECK(!g_cert_library);
  g_cert_library = new CertLibrary();
}

// static
void CertLibrary::Shutdown() {
  CHECK(g_cert_library);
  delete g_cert_library;
  g_cert_library = NULL;
}

// static
CertLibrary* CertLibrary::Get() {
  CHECK(g_cert_library) << "CertLibrary::Get() called before Initialize()";
  return g_cert_library;
}

// static
bool CertLibrary::IsInitialized() {
  return g_cert_library;
}

CertLibrary::CertLibrary() {
  CertLoader::Get()->AddObserver(this);
}

CertLibrary::~CertLibrary() {
  CertLoader::Get()->RemoveObserver(this);
}

void CertLibrary::AddObserver(CertLibrary::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CertLibrary::RemoveObserver(CertLibrary::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool CertLibrary::CertificatesLoading() const {
  return CertLoader::Get()->CertificatesLoading();
}

bool CertLibrary::CertificatesLoaded() const {
  return CertLoader::Get()->certificates_loaded();
}

bool CertLibrary::IsHardwareBacked() const {
  return CertLoader::Get()->IsHardwareBacked();
}

int CertLibrary::NumCertificates(CertType type) const {
  const net::CertificateList& cert_list = GetCertificateListForType(type);
  return static_cast<int>(cert_list.size());
}

string16 CertLibrary::GetCertDisplayStringAt(CertType type, int index) const {
  net::X509Certificate* cert = GetCertificateAt(type, index);
  bool hardware_backed = IsCertHardwareBackedAt(type, index);
  return GetDisplayString(cert, hardware_backed);
}

std::string CertLibrary::GetCertNicknameAt(CertType type, int index) const {
  net::X509Certificate* cert = GetCertificateAt(type, index);
  return x509_certificate_model::GetNickname(cert->os_cert_handle());
}

std::string CertLibrary::GetCertPkcs11IdAt(CertType type, int index) const {
  net::X509Certificate* cert = GetCertificateAt(type, index);
  return x509_certificate_model::GetPkcs11Id(cert->os_cert_handle());
}

bool CertLibrary::IsCertHardwareBackedAt(CertType type, int index) const {
  if (!CertLoader::Get()->IsHardwareBacked())
    return false;
  net::X509Certificate* cert = GetCertificateAt(type, index);
  std::string cert_token_name =
      x509_certificate_model::GetTokenName(cert->os_cert_handle());
  return cert_token_name == CertLoader::Get()->tpm_token_name();
}

int CertLibrary::GetCertIndexByNickname(CertType type,
                                        const std::string& nickname) const {
  int num_certs = NumCertificates(type);
  for (int index = 0; index < num_certs; ++index) {
    net::X509Certificate* cert = GetCertificateAt(type, index);
    net::X509Certificate::OSCertHandle cert_handle = cert->os_cert_handle();
    std::string nick = x509_certificate_model::GetNickname(cert_handle);
    if (nick == nickname)
      return index;
  }
  return -1;  // Not found.
}

int CertLibrary::GetCertIndexByPkcs11Id(CertType type,
                                        const std::string& pkcs11_id) const {
  int num_certs = NumCertificates(type);
  for (int index = 0; index < num_certs; ++index) {
    net::X509Certificate* cert = GetCertificateAt(type, index);
    net::X509Certificate::OSCertHandle cert_handle = cert->os_cert_handle();
    std::string id = x509_certificate_model::GetPkcs11Id(cert_handle);
    if (id == pkcs11_id)
      return index;
  }
  return -1;  // Not found.
}

void CertLibrary::OnCertificatesLoaded(const net::CertificateList& cert_list,
                                       bool initial_load) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  VLOG(1) << "CertLibrary::OnCertificatesLoaded: " << cert_list.size();
  certs_.clear();
  user_certs_.clear();
  server_certs_.clear();
  server_ca_certs_.clear();

  // Add certificates to the appropriate list.
  for (net::CertificateList::const_iterator iter = cert_list.begin();
       iter != cert_list.end(); ++iter) {
    certs_.push_back(iter->get());
    net::X509Certificate::OSCertHandle cert_handle =
        iter->get()->os_cert_handle();
    net::CertType type = x509_certificate_model::GetType(cert_handle);
    switch (type) {
      case net::USER_CERT:
        user_certs_.push_back(iter->get());
        break;
      case net::SERVER_CERT:
        server_certs_.push_back(iter->get());
        break;
      case net::CA_CERT: {
        // Exclude root CA certificates that are built into Chrome.
        std::string token_name =
            x509_certificate_model::GetTokenName(cert_handle);
        if (token_name != kRootCertificateTokenName)
          server_ca_certs_.push_back(iter->get());
        break;
      }
      default:
        break;
    }
  }

  // Perform locale-sensitive sorting by certificate name.
  UErrorCode error = U_ZERO_ERROR;
  scoped_ptr<icu::Collator> collator(icu::Collator::createInstance(
      icu::Locale(g_browser_process->GetApplicationLocale().c_str()), error));
  if (U_FAILURE(error))
    collator.reset();
  CertNameComparator cert_name_comparator(collator.get());
  std::sort(certs_.begin(), certs_.end(), cert_name_comparator);
  std::sort(user_certs_.begin(), user_certs_.end(), cert_name_comparator);
  std::sort(server_certs_.begin(), server_certs_.end(), cert_name_comparator);
  std::sort(server_ca_certs_.begin(), server_ca_certs_.end(),
            cert_name_comparator);

  VLOG(1) << "certs_: " << certs_.size();
  VLOG(1) << "user_certs_: " << user_certs_.size();
  VLOG(1) << "server_certs_: " << server_certs_.size();
  VLOG(1) << "server_ca_certs_: " << server_ca_certs_.size();

  FOR_EACH_OBSERVER(CertLibrary::Observer, observer_list_,
                    OnCertificatesLoaded(initial_load));
}

net::X509Certificate* CertLibrary::GetCertificateAt(CertType type,
                                                    int index) const {
  const net::CertificateList& cert_list = GetCertificateListForType(type);
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(cert_list.size()));
  return cert_list[index].get();
}

const net::CertificateList& CertLibrary::GetCertificateListForType(
    CertType type) const {
  if (type == CERT_TYPE_USER)
    return user_certs_;
  if (type == CERT_TYPE_SERVER)
    return server_certs_;
  if (type == CERT_TYPE_SERVER_CA)
    return server_ca_certs_;
  DCHECK(type == CERT_TYPE_DEFAULT);
  return certs_;
}

}  // chromeos
